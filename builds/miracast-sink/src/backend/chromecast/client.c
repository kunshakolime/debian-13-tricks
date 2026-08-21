#include "client.h"
#include "cast_channel.pb-c.h"

#include <string.h>
#include <gio/gio.h>

#define SOURCE_ID "sender-0"
#define RECEIVER_ID "receiver-0"

#define NS_CONNECTION "urn:x-cast:com.google.cast.tp.connection"
#define NS_HEARTBEAT "urn:x-cast:com.google.cast.tp.heartbeat"
#define NS_RECEIVER  "urn:x-cast:com.google.cast.receiver"
#define NS_MEDIA     "urn:x-cast:com.google.cast.media"

struct _MskChromecastClient
{
  GObject parent_instance;

  GSocketClient *socket_client;
  GSocketConnection *connection;
  GInputStream *input;
  GOutputStream *output;

  guint ping_timeout;
  guint request_id;

  GMainContext *main_ctx;
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

/* ---- protobuf encode --------------------------------------------------- */

static guint32
encode_varint (guint8 *buf, guint64 val)
{
  guint32 len = 0;
  while (val > 0x7f)
    {
      buf[len++] = (val & 0x7f) | 0x80;
      val >>= 7;
    }
  buf[len++] = val & 0x7f;
  return len;
}

static guint32
encode_field_header (guint8 *buf, guint32 field, guint32 wire_type)
{
  return encode_varint (buf, (guint64) field << 3 | wire_type);
}

static guint32
encode_string_field (guint8 *buf, guint32 field, const gchar *str)
{
  guint32 len = strlen (str);
  guint32 pos = 0;

  pos += encode_field_header (buf + pos, field, 2);
  pos += encode_varint (buf + pos, len);
  memcpy (buf + pos, str, len);
  pos += len;
  return pos;
}

static guint32
encode_varint_field (guint8 *buf, guint32 field, guint64 val)
{
  guint32 pos = 0;
  pos += encode_field_header (buf + pos, field, 0);
  pos += encode_varint (buf + pos, val);
  return pos;
}

static GByteArray *
build_cast_message (const gchar *source_id,
                    const gchar *dest_id,
                    const gchar *namespace,
                    const gchar *payload_utf8)
{
  guint8 buf[1024];
  guint32 pos = 0;
  guint32 msg_len;
  GByteArray *result;
  guint8 len_buf[4];

  /* field 1: protocol_version = 0 (CASTV2_1_0) */
  pos += encode_varint_field (buf + pos, 1, 0);

  /* field 2: source_id */
  pos += encode_string_field (buf + pos, 2, source_id);

  /* field 3: destination_id */
  pos += encode_string_field (buf + pos, 3, dest_id);

  /* field 4: namespace */
  pos += encode_string_field (buf + pos, 4, namespace);

  /* field 5: payload_type = 0 (STRING) */
  pos += encode_varint_field (buf + pos, 5, 0);

  /* field 6: payload_utf8 */
  pos += encode_string_field (buf + pos, 6, payload_utf8);

  result = g_byte_array_new ();

  /* 4-byte big-endian length prefix */
  msg_len = pos;
  len_buf[0] = (msg_len >> 24) & 0xff;
  len_buf[1] = (msg_len >> 16) & 0xff;
  len_buf[2] = (msg_len >> 8) & 0xff;
  len_buf[3] = msg_len & 0xff;
  g_byte_array_append (result, len_buf, 4);

  /* message body */
  g_byte_array_append (result, buf, pos);
  return result;
}

static gboolean
send_cast_message (MskChromecastClient *self,
                   const gchar *namespace,
                   const gchar *payload_utf8,
                   const gchar *dest_id)
{
  GByteArray *msg;
  GError *error = NULL;

  msg = build_cast_message (SOURCE_ID, dest_id, namespace, payload_utf8);

  if (!g_output_stream_write (self->output, msg->data, msg->len, NULL, &error))
    {
      g_warning ("Failed to send Cast message: %s", error->message);
      g_error_free (error);
      g_byte_array_unref (msg);
      return FALSE;
    }

  g_byte_array_unref (msg);
  return TRUE;
}

/* ---- protobuf decode --------------------------------------------------- */

static gchar *
decode_cast_message (const guint8 *data, gsize len,
                     gchar **out_namespace,
                     gchar **out_payload,
                     gchar **out_source_id,
                     gchar **out_dest_id)
{
  CastChannel__CastMessage *msg;
  gchar *payload = NULL;

  msg = cast_channel__cast_message__unpack (NULL, len, data);
  if (!msg)
    return NULL;

  if (out_namespace)
    *out_namespace = g_strdup (msg->namespace_ ? msg->namespace_ : "");
  if (out_payload)
    *out_payload = g_strdup (msg->payload_utf8 ? msg->payload_utf8 : "");
  if (out_source_id)
    *out_source_id = g_strdup (msg->source_id ? msg->source_id : "");
  if (out_dest_id)
    *out_dest_id = g_strdup (msg->destination_id ? msg->destination_id : "");

  if (msg->payload_utf8)
    payload = g_strdup (msg->payload_utf8);

  cast_channel__cast_message__free_unpacked (msg, NULL);
  return payload;
}

/* ---- TLS: skip cert verification --------------------------------------- */

static void
on_socket_client_event (GSocketClient *client G_GNUC_UNUSED,
                        GSocketClientEvent event,
                        GIOStream *connection,
                        gpointer user_data G_GNUC_UNUSED)
{
  if (event == G_SOCKET_CLIENT_TLS_HANDSHAKING)
    {
      GTlsClientConnection *tls = G_TLS_CLIENT_CONNECTION (connection);
      if (tls)
        g_tls_client_connection_set_validation_flags (tls, 0);
    }
}

/* ---- read loop --------------------------------------------------------- */

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
parse_and_emit_message (MskChromecastClient *self, const guint8 *data, gsize len)
{
  gchar *ns = NULL;
  gchar *payload = NULL;
  gchar *src = NULL;
  gchar *dst = NULL;
  gchar *json;

  json = decode_cast_message (data, len, &ns, &payload, &src, &dst);
  if (json)
    {
      g_signal_emit (self, signals[SIGNAL_MESSAGE], 0, ns, json);
      g_free (json);
    }
  g_free (ns);
  g_free (payload);
  g_free (src);
  g_free (dst);
}

/* ---- heartbeat --------------------------------------------------------- */

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

/* ---- reader thread ----------------------------------------------------- */

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

      parse_and_emit_message (self, (const guint8 *) msg, len);
      g_free (msg);
    }

  g_signal_emit (self, signals[SIGNAL_DISCONNECTED], 0);
  return NULL;
}

/* ---- GObject ----------------------------------------------------------- */

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
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
}

static void
msk_chromecast_client_init (MskChromecastClient *self)
{
  self->socket_client = g_socket_client_new ();
  g_socket_client_set_protocol (self->socket_client, G_SOCKET_PROTOCOL_TCP);
  g_socket_client_set_tls (self->socket_client, TRUE);
  g_signal_connect (self->socket_client, "event",
                    G_CALLBACK (on_socket_client_event), NULL);
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

  return send_cast_message (self, NS_CONNECTION,
                            "{\"type\":\"CONNECT\"}", destination_id);
}

gboolean
msk_chromecast_client_send_ping (MskChromecastClient *self,
                                 GError **error G_GNUC_UNUSED)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (self), FALSE);

  return send_cast_message (self, NS_HEARTBEAT,
                            "{\"type\":\"PING\"}", RECEIVER_ID);
}

gboolean
msk_chromecast_client_send_get_status (MskChromecastClient *self,
                                       GError **error G_GNUC_UNUSED)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (self), FALSE);

  return send_cast_message (self, NS_RECEIVER,
                            "{\"type\":\"GET_STATUS\"}", RECEIVER_ID);
}

gboolean
msk_chromecast_client_send_launch (MskChromecastClient *self,
                                   const gchar *app_id,
                                   GError **error G_GNUC_UNUSED)
{
  gchar *payload;
  gboolean ret;

  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (self), FALSE);
  g_return_val_if_fail (app_id != NULL, FALSE);

  payload = g_strdup_printf ("{\"type\":\"LAUNCH\",\"appId\":\"%s\"}", app_id);
  ret = send_cast_message (self, NS_RECEIVER, payload, RECEIVER_ID);
  g_free (payload);

  return ret;
}

gboolean
msk_chromecast_client_send_message (MskChromecastClient *self,
                                    const gchar *namespace,
                                    const gchar *payload_utf8,
                                    const gchar *destination_id,
                                    GError **error G_GNUC_UNUSED)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (self), FALSE);
  g_return_val_if_fail (namespace != NULL, FALSE);
  g_return_val_if_fail (payload_utf8 != NULL, FALSE);
  g_return_val_if_fail (destination_id != NULL, FALSE);

  return send_cast_message (self, namespace, payload_utf8, destination_id);
}
