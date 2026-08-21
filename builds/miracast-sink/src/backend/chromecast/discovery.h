#ifndef MSK_CHROMECAST_DISCOVERY_H
#define MSK_CHROMECAST_DISCOVERY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MSK_TYPE_CHROMECAST_DISCOVERY (msk_chromecast_discovery_get_type ())
G_DECLARE_FINAL_TYPE (MskChromecastDiscovery, msk_chromecast_discovery, MSK, CHROMECAST_DISCOVERY, GObject)

typedef struct _MskChromecastDevice
{
  gchar *name;
  gchar *host;
  guint16 port;
  gchar *id;
  gchar *model;
} MskChromecastDevice;

void msk_chromecast_device_free (MskChromecastDevice *device);

MskChromecastDiscovery *msk_chromecast_discovery_new (void);
void msk_chromecast_discovery_start (MskChromecastDiscovery *self);
void msk_chromecast_discovery_stop (MskChromecastDiscovery *self);

G_END_DECLS

#endif /* MSK_CHROMECAST_DISCOVERY_H */
