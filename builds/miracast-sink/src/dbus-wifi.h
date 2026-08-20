#ifndef MSK_DBUS_WIFI_H
#define MSK_DBUS_WIFI_H

#include <glib-object.h>
#include "common.h"

G_BEGIN_DECLS

#define MSK_TYPE_DBUS_WIFI (msk_dbus_wifi_get_type ())
G_DECLARE_FINAL_TYPE (MskDbusWifi, msk_dbus_wifi, MSK, DBUS_WIFI, GObject)

#define MSK_DBUS_WIFI_SIGNAL_LINK_ADDED       "link-added"
#define MSK_DBUS_WIFI_SIGNAL_PEER_ADDED       "peer-added"
#define MSK_DBUS_WIFI_SIGNAL_PEER_GO_NEG      "peer-go-neg"
#define MSK_DBUS_WIFI_SIGNAL_PEER_CONNECTED   "peer-connected"
#define MSK_DBUS_WIFI_SIGNAL_PEER_DISCONNECTED "peer-disconnected"

MskDbusWifi *msk_dbus_wifi_new (GError **error);
void msk_dbus_wifi_start (MskDbusWifi *self);

const char *msk_dbus_wifi_get_link_path     (MskDbusWifi *self);
void        msk_dbus_wifi_start_scanning    (MskDbusWifi *self);
void        msk_dbus_wifi_stop_scanning     (MskDbusWifi *self);
void        msk_dbus_wifi_set_friendly_name (MskDbusWifi *self, const char *name);
void        msk_dbus_wifi_accept_peer       (MskDbusWifi *self, const char *peer_path);
void        msk_dbus_wifi_disconnect_peer   (MskDbusWifi *self, const char *peer_path);

G_END_DECLS

#endif /* MSK_DBUS_WIFI_H */