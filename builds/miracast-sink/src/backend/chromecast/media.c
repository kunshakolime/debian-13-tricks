#include "media.h"
#include <json-glib/json-glib.h>

#define NS_MEDIA "urn:x-cast:com.google.cast.media"
#define SOURCE_ID "sender-0"
#define RECEIVER_ID "receiver-0"

struct _MskChromecastMedia
{
  GObject parent_instance;

  MskChromecastClient *client;
  gchar *media_session_id;
  gchar *transport_id;
  guint32 request_id;
  gdouble volume;
  gboolean muted;
};

G_DEFINE_TYPE (MskChromecastMedia, msk_chromecast_media, G_TYPE_OBJECT)

enum
{
  SIGNAL_STATUS_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
msk_chromecast_media_finalize (GObject *object)
{
  MskChromecastMedia *self = MSK_CHROMECAST_MEDIA (object);

  g_free (self->media_session_id);
  g_free (self->transport_id);
  g_clear_object (&self->client);

  G_OBJECT_CLASS (msk_chromecast_media_parent_class)->finalize (object);
}

static void
msk_chromecast_media_class_init (MskChromecastMediaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = msk_chromecast_media_finalize;

  signals[SIGNAL_STATUS_CHANGED] =
    g_signal_new ("status-changed",
                  MSK_TYPE_CHROMECAST_MEDIA, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
msk_chromecast_media_init (MskChromecastMedia *self)
{
  self->request_id = 1;
  self->volume = 1.0;
}

MskChromecastMedia *
msk_chromecast_media_new (MskChromecastClient *client)
{
  MskChromecastMedia *self;

  g_return_val_if_fail (MSK_IS_CHROMECAST_CLIENT (client), NULL);

  self = g_object_new (MSK_TYPE_CHROMECAST_MEDIA, NULL);
  self->client = g_object_ref (client);
  return self;
}

/* ---- status parsing ---------------------------------------------------- */

void
msk_chromecast_media_update_status (MskChromecastMedia *self,
                                    const gchar *payload_json)
{
  JsonParser *parser;
  JsonNode *root;
  JsonObject *obj;
  JsonObject *status;
  JsonArray *sessions;

  g_return_if_fail (MSK_IS_CHROMECAST_MEDIA (self));
  g_return_if_fail (payload_json != NULL);

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, payload_json, -1, NULL))
    {
      g_object_unref (parser);
      return;
    }

  root = json_parser_get_root (parser);
  if (!JSON_NODE_HOLDS_OBJECT (root))
    {
      g_object_unref (parser);
      return;
    }

  obj = json_node_get_object (root);
  status = json_object_get_object_member (obj, "status");
  if (!status)
    {
      g_object_unref (parser);
      return;
    }

  /* Extract volume */
  if (json_object_has_member (status, "volume"))
    {
      JsonObject *vol = json_object_get_object_member (status, "volume");
      if (vol)
        {
          if (json_object_has_member (vol, "level"))
            self->volume = json_object_get_double_member (vol, "level");
          if (json_object_has_member (vol, "muted"))
            self->muted = json_object_get_boolean_member (vol, "muted");
        }
    }

  /* Find active media session */
  sessions = json_object_get_array_member (status, "applications");
  if (sessions)
    {
      guint i, len = json_array_get_length (sessions);

      for (i = 0; i < len; i++)
        {
          JsonObject *app = json_array_get_object_element (sessions, i);
          const gchar *app_id;

          if (!app)
            continue;

          app_id = json_object_get_string_member (app, "appId");
          if (!app_id)
            continue;

          /* Media player app IDs */
          if (g_strcmp0 (app_id, "CC1AD845") == 0 ||  /* Default Media Receiver */
              g_strcmp0 (app_id, "3BAA0B12") == 0 ||  /* YouTube */
              g_str_has_prefix (app_id, "8A"))         /* Custom receivers */
            {
              g_free (self->transport_id);
              self->transport_id = g_strdup (
                json_object_get_string_member (app, "transportId"));

              if (json_object_has_member (app, "sessionId"))
                {
                  JsonArray *media_sessions;

                  g_free (self->media_session_id);
                  self->media_session_id = NULL;

                  /* Media sessions are typically in the mediaSession array */
                  media_sessions = json_object_get_array_member (app, "mediaSessionId");
                  if (media_sessions && json_array_get_length (media_sessions) > 0)
                    self->media_session_id = g_strdup (
                      json_array_get_string_element (media_sessions, 0));
                  else if (json_object_has_member (app, "mediaSessionId"))
                    self->media_session_id = g_strdup (
                      json_object_get_string_member (app, "mediaSessionId"));
                }

              break;
            }
        }
    }

  g_signal_emit (self, signals[SIGNAL_STATUS_CHANGED], 0, payload_json);
  g_object_unref (parser);
}

/* ---- media commands ---------------------------------------------------- */

gboolean
msk_chromecast_media_load_uri (MskChromecastMedia *self,
                                const gchar *uri,
                                const gchar *content_type,
                                GError **error G_GNUC_UNUSED)
{
  gchar *payload;
  gchar *req_id_str;
  gboolean ret;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  if (!self->transport_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media receiver");
      return FALSE;
    }

  req_id_str = g_strdup_printf ("%u", self->request_id++);

  payload = g_strdup_printf (
    "{\"type\":\"LOAD\","
    "\"media\":{\"contentId\":\"%s\",\"contentType\":\"%s\"},"
    "\"requestId\":\"%s\"}",
    uri, content_type ? content_type : "", req_id_str);

  ret = msk_chromecast_client_send_message (self->client,
                                             NS_MEDIA,
                                             payload,
                                             self->transport_id,
                                             error);
  g_free (payload);
  g_free (req_id_str);
  return ret;
}

gboolean
msk_chromecast_media_play (MskChromecastMedia *self, GError **error)
{
  gchar *payload;
  gchar *req_id_str;
  gboolean ret;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  req_id_str = g_strdup_printf ("%u", self->request_id++);
  payload = g_strdup_printf (
    "{\"type\":\"PLAY\","
    "\"mediaSessionId\":%s,"
    "\"requestId\":\"%s\"}",
    self->media_session_id, req_id_str);

  ret = msk_chromecast_client_send_message (self->client,
                                             NS_MEDIA,
                                             payload,
                                             self->transport_id,
                                             error);
  g_free (payload);
  g_free (req_id_str);
  return ret;
}

gboolean
msk_chromecast_media_pause (MskChromecastMedia *self, GError **error)
{
  gchar *payload;
  gchar *req_id_str;
  gboolean ret;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  req_id_str = g_strdup_printf ("%u", self->request_id++);
  payload = g_strdup_printf (
    "{\"type\":\"PAUSE\","
    "\"mediaSessionId\":%s,"
    "\"requestId\":\"%s\"}",
    self->media_session_id, req_id_str);

  ret = msk_chromecast_client_send_message (self->client,
                                             NS_MEDIA,
                                             payload,
                                             self->transport_id,
                                             error);
  g_free (payload);
  g_free (req_id_str);
  return ret;
}

gboolean
msk_chromecast_media_seek (MskChromecastMedia *self,
                            gint64 position_ms,
                            GError **error)
{
  gchar *payload;
  gchar *req_id_str;
  gchar *pos_str;
  gboolean ret;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  req_id_str = g_strdup_printf ("%u", self->request_id++);
  pos_str = g_strdup_printf ("%" G_GINT64_FORMAT, position_ms / 1000);
  payload = g_strdup_printf (
    "{\"type\":\"SEEK\","
    "\"mediaSessionId\":%s,"
    "\"currentTime\":%s,"
    "\"requestId\":\"%s\"}",
    self->media_session_id, pos_str, req_id_str);

  ret = msk_chromecast_client_send_message (self->client,
                                             NS_MEDIA,
                                             payload,
                                             self->transport_id,
                                             error);
  g_free (payload);
  g_free (pos_str);
  g_free (req_id_str);
  return ret;
}

gboolean
msk_chromecast_media_stop (MskChromecastMedia *self, GError **error)
{
  gchar *payload;
  gchar *req_id_str;
  gboolean ret;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  req_id_str = g_strdup_printf ("%u", self->request_id++);
  payload = g_strdup_printf (
    "{\"type\":\"STOP\","
    "\"mediaSessionId\":%s,"
    "\"requestId\":\"%s\"}",
    self->media_session_id, req_id_str);

  ret = msk_chromecast_client_send_message (self->client,
                                             NS_MEDIA,
                                             payload,
                                             self->transport_id,
                                             error);
  g_free (payload);
  g_free (req_id_str);
  return ret;
}

gboolean
msk_chromecast_media_set_volume (MskChromecastMedia *self,
                                  gdouble volume,
                                  GError **error)
{
  gchar *payload;
  gchar *req_id_str;
  gchar *vol_str;
  gboolean ret;

  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  req_id_str = g_strdup_printf ("%u", self->request_id++);
  vol_str = g_strdup_printf ("%.2f", volume);
  payload = g_strdup_printf (
    "{\"type\":\"SET_VOLUME\","
    "\"volume\":{\"level\":%s},"
    "\"requestId\":\"%s\"}",
    vol_str, req_id_str);

  ret = msk_chromecast_client_send_message (self->client,
                                             NS_MEDIA,
                                             payload,
                                             RECEIVER_ID,
                                             error);
  g_free (payload);
  g_free (vol_str);
  g_free (req_id_str);
  return ret;
}

const gchar *
msk_chromecast_media_get_session_id (MskChromecastMedia *self)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), NULL);
  return self->media_session_id;
}

const gchar *
msk_chromecast_media_get_transport_id (MskChromecastMedia *self)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), NULL);
  return self->transport_id;
}

gdouble
msk_chromecast_media_get_volume (MskChromecastMedia *self)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), 0.0);
  return self->volume;
}

gboolean
msk_chromecast_media_get_muted (MskChromecastMedia *self)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);
  return self->muted;
}
