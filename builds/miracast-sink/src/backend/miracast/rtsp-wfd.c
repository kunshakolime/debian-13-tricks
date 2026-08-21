#include "rtsp-wfd.h"

#include <gio/gio.h>
#include <string.h>

#define MSK_WFD_TOKEN "org.wfa.wfd1.0"

struct _MskRtspWfd
{
  GObject parent_instance;

  guint16 port;
  gboolean running;

  GSocketClient *client;
  GSocketConnection *conn;
  GDataInputStream *input;
  GDataOutputStream *output;

  GSource *io_source;
  gchar *url;      /* wfd_presentation_URL from source */
  gchar *session;  /* RTSP Session header */
};

G_DEFINE_TYPE (MskRtspWfd, msk_rtsp_wfd, G_TYPE_OBJECT)

enum
{
  SIGNAL_STREAM_READY,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

static void
msk_rtsp_wfd_disconnect (MskRtspWfd *self);

/* ---- message helpers -------------------------------------------------- */

static gchar *
message_body (const gchar *text, gsize len)
{
  const gchar *sep = strstr (text, "\r\n\r\n");
  gsize body_len;

  if (!sep)
    return NULL;
  sep += 4;
  body_len = len - (sep - text);
  if (body_len == 0)
    return NULL;
  return g_strndup (sep, body_len);
}

static const gchar *
message_value (const gchar *text, const gchar *header)
{
  gchar *pattern = g_strdup_printf ("\r\n%s:", header);
  const gchar *hit = strstr (text, pattern);

  g_free (pattern);
  if (!hit)
    return NULL;
  hit += 2 + strlen (header) + 1;
  while (*hit == ' ' || *hit == '\t')
    hit++;
  return hit;
}

static gboolean
send_message (MskRtspWfd *self, const gchar *text)
{
  g_debug ("RTSP >>\n%s", text);
  return g_data_output_stream_put_string (self->output, text, NULL, NULL);
}

static void
send_reply (MskRtspWfd *self, gint code, const gchar *reason,
            const gchar *extra)
{
  GString *s = g_string_new (NULL);

  g_string_append_printf (s, "RTSP/1.0 %d %s\r\nCSeq: 1\r\n",
                          code, reason);
  if (extra)
    g_string_append (s, extra);
  g_string_append (s, "\r\n");
  send_message (self, s->str);
  g_string_free (s, TRUE);
}

static void
send_request (MskRtspWfd *self, const gchar *method, const gchar *target,
              const gchar *extra)
{
  GString *s = g_string_new (NULL);

  g_string_append_printf (s, "%s %s RTSP/1.0\r\nCSeq: 1\r\n",
                          method, target);
  if (extra)
    g_string_append (s, extra);
  g_string_append (s, "\r\n");
  send_message (self, s->str);
  g_string_free (s, TRUE);
}

/* ---- WFD handshake handlers ------------------------------------------- */

static void
handle_options (MskRtspWfd *self, G_GNUC_UNUSED const gchar *msg)
{
  send_reply (self, 200, "OK",
              "Public: org.wfa.wfd1.0, GET_PARAMETER, SET_PARAMETER\r\n");

  send_request (self, "OPTIONS", "*",
                "Require: org.wfa.wfd1.0\r\n");
}

static void
handle_get_parameter (MskRtspWfd *self, G_GNUC_UNUSED const gchar *msg)
{
  GString *body = g_string_new (NULL);
  const gchar *content_protection = "none";
  const gchar *video_formats =
    "00 00 03 10 0001ffff 1fffffff 00001fff 00 0000 0000 10 none none";
  const gchar *audio_codecs = "AAC 00000007 00";
  gchar rtp_ports[64];

  g_snprintf (rtp_ports, sizeof (rtp_ports),
              "RTP/AVP/UDP;unicast %d 0 mode=play", self->port);

  g_string_append_printf (body, "wfd_content_protection: %s\r\n",
                          content_protection);
  g_string_append_printf (body, "wfd_video_formats: %s\r\n", video_formats);
  g_string_append_printf (body, "wfd_audio_codecs: %s\r\n", audio_codecs);
  g_string_append_printf (body, "wfd_client_rtp_ports: %s\r\n", rtp_ports);

  {
    GString *extra = g_string_new (NULL);
    g_string_append_printf (extra, "Content-Type: text/parameters\r\n");
    g_string_append_printf (extra, "Content-Length: %u\r\n",
                            (guint) body->len);
    send_reply (self, 200, "OK", extra->str);
    g_string_free (extra, TRUE);
  }

  send_message (self, body->str);
  g_string_free (body, TRUE);
}

static void
handle_set_parameter (MskRtspWfd *self, const gchar *msg)
{
  const gchar *presentation_url;
  const gchar *video_formats;
  const gchar *trigger;

  send_reply (self, 200, "OK", NULL);

  presentation_url = message_value (msg, "wfd_presentation_URL");
  if (presentation_url)
    {
      gchar *value = message_body (msg, strlen (msg));
      const gchar *url = value ? strstr (value, "rtsp://") : NULL;
      gchar *end;

      if (url)
        {
          end = strpbrk (url, " \r\n");
          g_free (self->url);
          self->url = end ? g_strndup (url, end - url)
                          : g_strdup (url);
          g_debug ("RTSP presentation URL: %s", self->url);
        }
      g_free (value);
    }

  video_formats = message_value (msg, "wfd_video_formats");
  if (video_formats)
    g_debug ("RTSP source video formats: %s", video_formats);

  trigger = message_value (msg, "wfd_trigger_method");
  if (trigger && strstr (trigger, "SETUP"))
    {
      gchar transport[128];

      g_snprintf (transport, sizeof (transport),
                  "Transport: RTP/AVP/UDP;unicast;client_port=%d\r\n",
                  self->port);
      send_request (self, "SETUP",
                    self->url ? self->url : "rtsp://localhost/wfd1.0",
                    transport);
    }
}

static void
handle_reply (MskRtspWfd *self, const gchar *msg)
{
  const gchar *session;

  session = message_value (msg, "Session");
  if (session)
    {
      gchar *end = strpbrk (session, ";\r\n");

      g_free (self->session);
      self->session = end ? g_strndup (session, end - session)
                          : g_strdup (session);
    }

  /* SETUP reply with a session means the stream is up */
  if (self->session && self->url &&
      g_str_has_prefix (msg, "RTSP/1.0 200"))
    {
      gchar transport[128];

      g_snprintf (transport, sizeof (transport),
                  "Session: %s\r\n", self->session);
      send_request (self, "PLAY", self->url, transport);

      g_signal_emit (self, signals[SIGNAL_STREAM_READY], 0);
    }
}

/* ---- socket I/O ------------------------------------------------------- */

static gboolean
on_input (gpointer user_data)
{
  MskRtspWfd *self = MSK_RTSP_WFD (user_data);
  gchar *line;
  GString *msg = g_string_new (NULL);
  gsize len;
  GError *error = NULL;

  /* read status line */
  line = g_data_input_stream_read_line (self->input, &len, NULL, &error);
  if (error || !line)
    {
      g_clear_error (&error);
      if (line)
        g_free (line);
      if (self->running)
        g_signal_emit (self, signals[SIGNAL_STREAM_READY], 0);
      msk_rtsp_wfd_disconnect (self);
      g_string_free (msg, TRUE);
      return G_SOURCE_REMOVE;
    }

  g_string_append (msg, line);
  g_string_append (msg, "\r\n");
  g_free (line);

  /* headers */
  {
    gint64 content_length = -1;

    for (;;)
      {
        line = g_data_input_stream_read_line (self->input, &len, NULL, &error);
        if (error || !line)
          {
            g_clear_error (&error);
            g_string_free (msg, TRUE);
            if (self->running)
              g_signal_emit (self, signals[SIGNAL_STREAM_READY], 0);
            msk_rtsp_wfd_disconnect (self);
            return G_SOURCE_REMOVE;
          }
        if (len == 0)
          {
            g_free (line);
            /* blank line: keep the header/body separator so message_body()
             * and message_value() can find what they look for */
            g_string_append (msg, "\r\n");
            break;
          }
        g_string_append (msg, line);
        g_string_append (msg, "\r\n");
        if (g_ascii_strncasecmp (line, "Content-Length:", 15) == 0)
          content_length = g_ascii_strtoll (line + 15, NULL, 10);
        g_free (line);
      }

    /* read the message body so wfd_presentation_URL / wfd_trigger_method
     * are visible to the handlers below */
    if (content_length > 0)
      {
        gchar buf[1024];
        gint64 remaining = content_length;

        while (remaining > 0)
          {
            gsize want = MIN (remaining, (gint64) sizeof (buf));
            gssize got = g_input_stream_read (
                           G_INPUT_STREAM (self->input),
                           buf, want, NULL, &error);
            if (error || got <= 0)
              {
                g_clear_error (&error);
                g_string_free (msg, TRUE);
                if (self->running)
                  g_signal_emit (self, signals[SIGNAL_STREAM_READY], 0);
                msk_rtsp_wfd_disconnect (self);
                return G_SOURCE_REMOVE;
              }
            g_string_append_len (msg, buf, got);
            remaining -= got;
          }
      }
  }

  g_debug ("RTSP <<\n%s", msg->str);

  if (g_str_has_prefix (msg->str, "OPTIONS"))
    handle_options (self, msg->str);
  else if (g_str_has_prefix (msg->str, "GET_PARAMETER"))
    handle_get_parameter (self, msg->str);
  else if (g_str_has_prefix (msg->str, "SET_PARAMETER"))
    handle_set_parameter (self, msg->str);
  else if (g_str_has_prefix (msg->str, "RTSP/1.0"))
    handle_reply (self, msg->str);

  g_string_free (msg, TRUE);
  return G_SOURCE_CONTINUE;
}

static void
on_connected (G_GNUC_UNUSED GObject *source, GAsyncResult *res,
              gpointer user_data)
{
  MskRtspWfd *self = MSK_RTSP_WFD (user_data);
  GError *error = NULL;

  self->conn = g_socket_client_connect_to_host_finish (self->client, res,
                                                       &error);
  if (error)
    {
      g_warning ("RTSP connect failed: %s", error->message);
      g_clear_error (&error);
      return;
    }

  self->input = g_data_input_stream_new (g_io_stream_get_input_stream (
                                           G_IO_STREAM (self->conn)));
  self->output = g_data_output_stream_new (g_io_stream_get_output_stream (
                                             G_IO_STREAM (self->conn)));
  g_data_input_stream_set_newline_type (self->input,
                                        G_DATA_STREAM_NEWLINE_TYPE_CR_LF);

  self->io_source = g_pollable_input_stream_create_source (
                     G_POLLABLE_INPUT_STREAM (
                       g_io_stream_get_input_stream (G_IO_STREAM (self->conn))),
                     NULL);
  g_source_set_callback (self->io_source, on_input, self, NULL);
  g_source_attach (self->io_source, NULL);
}

static void
msk_rtsp_wfd_disconnect (MskRtspWfd *self)
{
  if (self->io_source)
    {
      g_source_destroy (self->io_source);
      g_source_unref (self->io_source);
      self->io_source = NULL;
    }
  g_clear_object (&self->conn);
  g_clear_object (&self->input);
  g_clear_object (&self->output);
}

void
msk_rtsp_wfd_start (G_GNUC_UNUSED MskRtspWfd *self)
{
}

void
msk_rtsp_wfd_stop (MskRtspWfd *self)
{
  self->running = FALSE;
  msk_rtsp_wfd_disconnect (self);
}

static void
msk_rtsp_wfd_finalize (GObject *object)
{
  MskRtspWfd *self = MSK_RTSP_WFD (object);

  msk_rtsp_wfd_disconnect (self);
  g_clear_object (&self->client);
  g_free (self->url);
  g_free (self->session);

  G_OBJECT_CLASS (msk_rtsp_wfd_parent_class)->finalize (object);
}

static void
msk_rtsp_wfd_class_init (MskRtspWfdClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = msk_rtsp_wfd_finalize;

  signals[SIGNAL_STREAM_READY] =
    g_signal_new (MSK_RTSP_WFD_SIGNAL_STREAM_READY,
                  MSK_TYPE_RTSP_WFD, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
msk_rtsp_wfd_init (MskRtspWfd *self)
{
  self->client = g_socket_client_new ();
  g_socket_client_set_protocol (self->client, G_SOCKET_PROTOCOL_TCP);
}

MskRtspWfd *
msk_rtsp_wfd_new (guint16 port, G_GNUC_UNUSED GError **error)
{
  MskRtspWfd *self;

  self = g_object_new (MSK_TYPE_RTSP_WFD, NULL);
  self->port = port ? port : MSK_DEFAULT_PORT;
  self->running = TRUE;
  return self;
}

/* connect out to the Miracast source's RTSP port */
void
msk_rtsp_wfd_connect_peer (MskRtspWfd *self, const gchar *peer_address)
{
  if (!self->running)
    return;

  msk_rtsp_wfd_disconnect (self);

  g_socket_client_connect_to_host_async (self->client,
                                         peer_address, self->port,
                                         NULL, on_connected, self);
}