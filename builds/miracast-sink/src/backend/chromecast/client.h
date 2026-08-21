#ifndef MSK_CHROMECAST_CLIENT_H
#define MSK_CHROMECAST_CLIENT_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define MSK_TYPE_CHROMECAST_CLIENT (msk_chromecast_client_get_type ())
G_DECLARE_FINAL_TYPE (MskChromecastClient, msk_chromecast_client, MSK, CHROMECAST_CLIENT, GObject)

MskChromecastClient *msk_chromecast_client_new (void);
gboolean msk_chromecast_client_connect (MskChromecastClient *self,
                                         const gchar *host,
                                         guint16 port,
                                         GError **error);
void msk_chromecast_client_disconnect (MskChromecastClient *self);
gboolean msk_chromecast_client_is_connected (MskChromecastClient *self);

gboolean msk_chromecast_client_send_connect (MskChromecastClient *self,
                                              const gchar *destination_id,
                                              GError **error);
gboolean msk_chromecast_client_send_ping (MskChromecastClient *self,
                                           GError **error);
gboolean msk_chromecast_client_send_get_status (MskChromecastClient *self,
                                                 GError **error);
gboolean msk_chromecast_client_send_launch (MskChromecastClient *self,
                                             const gchar *app_id,
                                             GError **error);
gboolean msk_chromecast_client_send_message (MskChromecastClient *self,
                                              const gchar *namespace,
                                              const gchar *payload_utf8,
                                              const gchar *destination_id,
                                              GError **error);

G_END_DECLS

#endif /* MSK_CHROMECAST_CLIENT_H */
