#include "media.h"

#define NS_MEDIA "urn:x-cast:com.google.cast.media"

struct _MskChromecastMedia
{
  GObject parent_instance;

  MskChromecastClient *client;
  gchar *media_session_id;
  guint32 request_id;
};

enum
{
  SIGNAL_MEDIA_STATUS,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static gboolean
send_media_command (MskChromecastMedia *self, const gchar *payload)
{
  /* Media commands go to the media session, not receiver-0 */
  /* We need to send to the transportId from MEDIA_STATUS */
  /* For simplicity, we'll send to receiver-0 for now */
  return msk_chromecast_client_send_launch (self->client, "CC1AD845", NULL);
}

static void
msk_chromecast_media_finalize (GObject *object)
{
  MskChromecastMedia *self = MSK_CHROMECAST_MEDIA (object);

  g_free (self->media_session_id);

  G_OBJECT_CLASS (msk_chromecast_media_parent_class)->finalize (object);
}

static void
msk_chromecast_media_class_init (MskChromecastMediaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = msk_chromecast_media_finalize;

  signals[SIGNAL_MEDIA_STATUS] =
    g_signal_new ("media-status",
                  MSK_TYPE_CHROMECAST_MEDIA, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
msk_chromecast_media_init (MskChromecastMedia *self)
{
  self->request_id = 1;
}

MskChromecastMedia *
msk_chromecast_media_new (MskChromecastClient *client)
{
  MskChromecastMedia *self;

  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (client), NULL);

  self = g_object_new (MSK_TYPE_CHROMECAST_MEDIA, NULL);
  self->client = client;
  return self;
}

gboolean
msk_chromecast_media_load_uri (MskChromecastMedia *self,
                               const gchar *uri,
                               const gchar *content_type,
                               GError **error)
{
  gchar *payload;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  /* TODO: Send LOAD command to media namespace with proper mediaSessionId */
  payload = g_strdup_printf (
    "{\"type\":\"LOAD\","
    "\"media\":{\"contentId\":\"%s\",\"contentType\":\"%s\"},"
    "\"autoplay\":true,"
    "\"requestId\":%u}",
    uri, content_type ? content_type : "video/mp4",
    self->request_id++);

  gboolean ret = send_media_command (self, payload);
  g_free (payload);

  return ret;
}

gboolean
msk_chromecast_media_play (MskChromecastMedia *self, GError **error)
{
  gchar *payload;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  payload = g_strdup_printf (
    "{\"type\":\"PLAY\","
    "\"mediaSessionId\":\"%s\","
    "\"requestId\":%u}",
    self->media_session_id, self->request_id++);

  gboolean ret = send_media_command (self, payload);
  g_free (payload);

  return ret;
}

gboolean
msk_chromecast_media_pause (MskChromecastMedia *self, GError **error)
{
  gchar *payload;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  payload = g_strdup_printf (
    "{\"type\":\"PAUSE\","
    "\"mediaSessionId\":\"%s\","
    "\"requestId\":%u}",
    self->media_session_id, self->request_id++);

  gboolean ret = send_media_command (self, payload);
  g_free (payload);

  return ret;
}

gboolean
msk_chromecast_media_seek (MskChromecastMedia *self,
                           gint64 position_ms,
                           GError **error)
{
  gchar *payload;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  payload = g_strdup_printf (
    "{\"type\":\"SEEK\","
    "\"mediaSessionId\":\"%s\","
    "\"currentTime\":%f,"
    "\"requestId\":%u}",
    self->media_session_id,
    position_ms / 1000.0,
    self->request_id++);

  gboolean ret = send_media_command (self, payload);
  g_free (payload);

  return ret;
}

gboolean
msk_chromecast_media_stop (MskChromecastMedia *self, GError **error)
{
  gchar *payload;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  payload = g_strdup_printf (
    "{\"type\":\"STOP\","
    "\"mediaSessionId\":\"%s\","
    "\"requestId\":%u}",
    self->media_session_id, self->request_id++);

  gboolean ret = send_media_command (self, payload);
  g_free (payload);

  return ret;
}
