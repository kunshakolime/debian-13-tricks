#include "client.h"
#include "cast_channel.pb-c.h"

#include <string.h>

#define SOURCE_ID "sender-0"
#define RECEIVER_ID "receiver-0"

#define NS_CONNECTION "urn:x-cast:com.google.cast.tp.connection"
#define NS_HEARTBEAT "urn:x-cast:com.google.cast.tp.heartbeat"
#define NS_RECEIVER "urn:x-cast:com.google.cast.receiver"

struct _MskChromecastClient
{
  GObject parent_instance;

  GSocketClient *socket_client;
  GSocketConnection *connection;
  GInputStream *input;
  GOutputStream *output;

  guint ping_timeout;
  guint request_id;
};

G_DEFINE_TYPE (MskChromecastClient, msk_chromecast_client, G_TYPE_OBJECT)

enum
{
  SIGNAL_CONNECTED,
  SIGNAL_DISCONNECTED,
  SIGNAL_MESSAGE,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static gboolean
send_raw_message (MskChromecastClient *self,
                  const gchar *namespace,
                  const gchar *payload_utf8,
                  const gchar *destination_id)
{
  guint32 source_id_len = strlen (SOURCE_ID);
  guint32 dest_id_len = strlen (destination_id);
  guint32 namespace_len = strlen (namespace);
  guint32 payload_len = strlen (payload_utf8);
  guchar header[1];

  GByteArray *msg = g_byte_array_new ();

  /* protocol_version = CASTV2_1_0 (field 1, varint) */
  g_byte_array_append (msg, (guint8 *) "\x08\x00", 2);

  /* source_id (field 2, length-delimited) */
  g_byte_array_append (msg, (guint8 *) "\x12", 1);
  header[0] = source_id_len;
  g_byte_array_append (msg, header, 1);
  g_byte_array_append (msg, (guint8 *) SOURCE_ID, source_id_len);

  /* destination_id (field 3, length-delimited) */
  g_byte_array_append (msg, (guint8 *) "\x1a", 1);
  header[0] = dest_id_len;
  g_byte_array_append (msg, header, 1);
  g_byte_array_append (msg, (guint8 *) destination_id, dest_id_len);

  /* namespace (field 4, length-delimited) */
  g_byte_array_append (msg, (guint8 *) "\x22", 1);
  header[0] = namespace_len;
  g_byte_array_append (msg, header, 1);
  g_byte_array_append (msg, (guint8 *) namespace, namespace_len);

  /* payload_type = STRING (field 5, varint) */
  g_byte_array_append (msg, (guint8 *) "\x28\x00", 2);

  /* payload_utf8 (field 6, length-delimited) */
  g_byte_array_append (msg, (guint8 *) "\x32", 1);
  header[0] = payload_len;
  g_byte_array_append (msg, header, 1);
  g_byte_array_append (msg, (guint8 *) payload_utf8, payload_len);

  /* Send 4-byte big-endian length prefix */
  guint32 total_len = msg->len;
  guchar len_be[4];
  len_be[0] = (total_len >> 24) & 0xff;
  len_be[1] = (total_len >> 16) & 0xff;
  len_be[2] = (total_len >> 8) & 0xff;
  len_be[3] = total_len & 0xff;

  if (!g_output_stream_write (self->output, len_be, 4, NULL, NULL))
    {
      g_byte_array_unref (msg);
      return FALSE;
    }

  if (!g_output_stream_write (self->output, msg->data, msg->len, NULL, NULL))
    {
      g_byte_array_unref (msg);
      return FALSE;
    }

  g_byte_array_unref (msg);
  return TRUE;
}

static gchar *
read_length_prefixed_message (MskChromecastClient *self, gsize *out_len)
{
  guchar len_be[4];
  gssize got;
  guint32 msg_len;
  guchar *buf;

  got = g_input_stream_read (self->input, len_be, 4, NULL, NULL);
  if (got != 4)
    return NULL;

  msg_len = (len_be[0] << 24) | (len_be[1] << 16) | (len_be[2] << 8) | len_be[3];

  if (msg_len == 0 || msg_len > 1024 * 1024)
    return NULL;

  buf = g_malloc (msg_len);
  got = g_input_stream_read (self->input, buf, msg_len, NULL, NULL);
  if (got != (gssize) msg_len)
    {
      g_free (buf);
      return NULL;
    }

  *out_len = msg_len;
  return (gchar *) buf;
}

static void
parse_and_emit_message (MskChromecastClient *self, const gchar *data, gsize len)
{
  const gchar *json_start = NULL;
  gsize i;

  for (i = 0; i < len; i++)
    {
      if (data[i] == '{')
        {
          json_start = data + i;
          break;
        }
    }

  if (json_start)
    g_signal_emit (self, signals[SIGNAL_MESSAGE], 0, json_start);
}

static gboolean
on_ping_timeout (gpointer user_data)
{
  MskChromecastClient *self = MSK_CHROMECAST_CLIENT (user_data);
  GError *error = NULL;

  if (!msk_chromecast_client_send_ping (self, &error))
    {
      g_warning ("Heartbeat ping failed: %s", error->message);
      g_error_free (error);
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static gpointer
read_loop (gpointer user_data)
{
  MskChromecastClient *self = MSK_CHROMECAST_CLIENT (user_data);

  while (self->connection)
    {
      gsize len;
      gchar *msg = read_length_prefixed_message (self, &len);
      if (!msg)
        break;

      parse_and_emit_message (self, msg, len);
      g_free (msg);
    }

  g_signal_emit (self, signals[SIGNAL_DISCONNECTED], 0);
  return NULL;
}

static void
msk_chromecast_client_finalize (GObject *object)
{
  MskChromecastClient *self = MSK_CHROMECAST_CLIENT (object);

  msk_chromecast_client_disconnect (self);

  G_OBJECT_CLASS (msk_chromecast_client_parent_class)->finalize (object);
}

static void
msk_chromecast_client_class_init (MskChromecastClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = msk_chromecast_client_finalize;

  signals[SIGNAL_CONNECTED] =
    g_signal_new ("connected",
                  MSK_TYPE_CHROMECAST_CLIENT, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[SIGNAL_DISCONNECTED] =
    g_signal_new ("disconnected",
                  MSK_TYPE_CHROMECAST_CLIENT, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[SIGNAL_MESSAGE] =
    g_signal_new ("message",
                  MSK_TYPE_CHROMECAST_CLIENT, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
msk_chromecast_client_init (MskChromecastClient *self)
{
  self->socket_client = g_socket_client_new ();
  g_socket_client_set_protocol (self->socket_client, G_SOCKET_PROTOCOL_TCP);
  self->request_id = 1;
}

MskChromecastClient *
msk_chromecast_client_new (void)
{
  return g_object_new (MSK_TYPE_CHROMECAST_CLIENT, NULL);
}

gboolean
msk_chromecast_client_connect (MskChromecastClient *self,
                               const gchar *host,
                               guint16 port,
                               GError **error)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (self), FALSE);
  g_return_val_if_fail (host != NULL, FALSE);

  self->connection = g_socket_client_connect_to_host (self->socket_client,
                                                       host, port,
                                                       NULL, error);
  if (!self->connection)
    return FALSE;

  self->input = g_io_stream_get_input_stream (G_IO_STREAM (self->connection));
  self->output = g_io_stream_get_output_stream (G_IO_STREAM (self->connection));

  g_signal_emit (self, signals[SIGNAL_CONNECTED], 0);

  g_thread_new ("chromecast-read", read_loop, self);

  self->ping_timeout = g_timeout_add_seconds (5, on_ping_timeout, self);

  return TRUE;
}

void
msk_chromecast_client_disconnect (MskChromecastClient *self)
{
  g_return_if_fail (MSK_IS_CHROMECAST_CLIENT (self));

  if (self->ping_timeout)
    {
      g_source_remove (self->ping_timeout);
      self->ping_timeout = 0;
    }

  if (self->connection)
    {
      g_io_stream_close (G_IO_STREAM (self->connection), NULL, NULL);
      g_object_unref (self->connection);
      self->connection = NULL;
    }

  self->input = NULL;
  self->output = NULL;
}

gboolean
msk_chromecast_client_is_connected (MskChromecastClient *self)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (self), FALSE);
  return self->connection != NULL;
}

gboolean
msk_chromecast_client_send_connect (MskChromecastClient *self,
                                    const gchar *destination_id,
                                    GError **error G_GNUC_UNUSED)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (self), FALSE);

  return send_raw_message (self, NS_CONNECTION,
                           "{\"type\":\"CONNECT\"}", destination_id);
}

gboolean
msk_chromecast_client_send_ping (MskChromecastClient *self,
                                 GError **error G_GNUC_UNUSED)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (self), FALSE);

  return send_raw_message (self, NS_HEARTBEAT,
                           "{\"type\":\"PING\"}", RECEIVER_ID);
}

gboolean
msk_chromecast_client_send_get_status (MskChromecastClient *self,
                                       GError **error G_GNUC_UNUSED)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (self), FALSE);

  return send_raw_message (self, NS_RECEIVER,
                           "{\"type\":\"GET_STATUS\"}", RECEIVER_ID);
}

gboolean
msk_chromecast_client_send_launch (MskChromecastClient *self,
                                   const gchar *app_id,
                                   GError **error G_GNUC_UNUSED)
{
  gchar *payload;

  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (self), FALSE);
  g_return_val_if_fail (app_id != NULL, FALSE);

  payload = g_strdup_printf ("{\"type\":\"LAUNCH\",\"appId\":\"%s\"}", app_id);
  gboolean ret = send_raw_message (self, NS_RECEIVER, payload, RECEIVER_ID);
  g_free (payload);

  return ret;
}
