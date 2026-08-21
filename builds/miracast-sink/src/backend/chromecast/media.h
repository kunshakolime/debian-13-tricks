#ifndef MSK_CHROMECAST_MEDIA_H
#define MSK_CHROMECAST_MEDIA_H

#include <glib-object.h>
#include "client.h"

G_BEGIN_DECLS

#define MSK_TYPE_CHROMECAST_MEDIA (msk_chromecast_media_get_type ())
G_DECLARE_FINAL_TYPE (MskChromecastMedia, msk_chromecast_media, MSK, CHROMECAST_MEDIA, GObject)

MskChromecastMedia *msk_chromecast_media_new (MskChromecastClient *client);

gboolean msk_chromecast_media_load_uri (MskChromecastMedia *self,
                                         const gchar *uri,
                                         const gchar *content_type,
                                         GError **error);
gboolean msk_chromecast_media_play (MskChromecastMedia *self, GError **error);
gboolean msk_chromecast_media_pause (MskChromecastMedia *self, GError **error);
gboolean msk_chromecast_media_seek (MskChromecastMedia *self,
                                     gint64 position_ms,
                                     GError **error);
gboolean msk_chromecast_media_stop (MskChromecastMedia *self, GError **error);

G_END_DECLS

#endif /* MSK_CHROMECAST_MEDIA_H */
