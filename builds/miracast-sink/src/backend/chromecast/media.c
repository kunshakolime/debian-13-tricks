#include "media.h"

struct _MskChromecastMedia
{
  GObject parent_instance;

  MskChromecastClient *client;
  gchar *media_session_id;
  guint32 request_id;
};

G_DEFINE_TYPE (MskChromecastMedia, msk_chromecast_media, G_TYPE_OBJECT)

static void
msk_chromecast_media_finalize (GObject *object)
{
  MskChromecastMedia *self = MSK_CHROMECAST_MEDIA (object);

  g_free (self->media_session_id);
  g_clear_object (&self->client);

  G_OBJECT_CLASS (msk_chromecast_media_parent_class)->finalize (object);
}

static void
msk_chromecast_media_class_init (MskChromecastMediaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = msk_chromecast_media_finalize;
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
  self->client = g_object_ref (client);
  return self;
}

gboolean
msk_chromecast_media_load_uri (MskChromecastMedia *self,
                               const gchar *uri,
                               const gchar *content_type,
                               GError **error G_GNUC_UNUSED)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  /* TODO: Send LOAD command to media namespace */
  (void) self;
  (void) content_type;
  return TRUE;
}

gboolean
msk_chromecast_media_play (MskChromecastMedia *self, GError **error)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  return TRUE;
}

gboolean
msk_chromecast_media_pause (MskChromecastMedia *self, GError **error)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  return TRUE;
}

gboolean
msk_chromecast_media_seek (MskChromecastMedia *self,
                           gint64 position_ms,
                           GError **error)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  (void) position_ms;
  return TRUE;
}

gboolean
msk_chromecast_media_stop (MskChromecastMedia *self, GError **error)
{
  g_return_val_if_fail (MSK_IS_CHROMECAST_MEDIA (self), FALSE);

  if (!self->media_session_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "No active media session");
      return FALSE;
    }

  return TRUE;
}
