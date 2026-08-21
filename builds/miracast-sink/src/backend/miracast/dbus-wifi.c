#include "dbus-wifi.h"

#include <gio/gio.h>

#define MSK_WIFI_OBJECT_MANAGER "org.freedesktop.DBus.ObjectManager"
#define MSK_WIFI_PROPERTIES     "org.freedesktop.DBus.Properties"
#define MSK_WIFI_MANAGER_IFACE  "org.freedesktop.miracle.wifi.Manager"

struct _MskDbusWifi
{
  GObject parent_instance;

  GDBusConnection *connection;
  gchar *link_path;
  gchar *link_name;
  gboolean scanning;
  gboolean running;
};

G_DEFINE_TYPE (MskDbusWifi, msk_dbus_wifi, G_TYPE_OBJECT)

enum
{
  SIGNAL_LINK_ADDED,
  SIGNAL_PEER_ADDED,
  SIGNAL_PEER_GO_NEG,
  SIGNAL_PEER_CONNECTED,
  SIGNAL_PEER_DISCONNECTED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

static void
emit_peer_connected (MskDbusWifi *self, const gchar *path, const gchar *remote)
{
  g_signal_emit (self, signals[SIGNAL_PEER_CONNECTED], 0, path, remote);
}

static void
emit_peer_disconnected (MskDbusWifi *self, const gchar *path)
{
  g_signal_emit (self, signals[SIGNAL_PEER_DISCONNECTED], 0, path);
}

/* ---- properties lookup helpers ---------------------------------------- */

static gchar *
get_str_prop (GDBusConnection *conn,
              const gchar *path,
              const gchar *iface,
              const gchar *prop)
{
  GVariant *v, *value;
  gchar *out = NULL;

  v = g_dbus_connection_call_sync (conn,
                                   MSK_WIFI_BUS_NAME,
                                   path,
                                   MSK_WIFI_PROPERTIES,
                                   "Get",
                                   g_variant_new ("(ss)", iface, prop),
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   5000, NULL, NULL);
  if (v)
    {
      g_variant_get (v, "(v)", &value);
      if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        out = g_strdup (g_variant_get_string (value, NULL));
      g_variant_unref (value);
      g_variant_unref (v);
    }

  return out;
}

static gboolean
get_bool_prop (GDBusConnection *conn,
               const gchar *path,
               const gchar *iface,
               const gchar *prop)
{
  GVariant *v, *value;
  gboolean out = FALSE;

  v = g_dbus_connection_call_sync (conn,
                                   MSK_WIFI_BUS_NAME,
                                   path,
                                   MSK_WIFI_PROPERTIES,
                                   "Get",
                                   g_variant_new ("(ss)", iface, prop),
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   5000, NULL, NULL);
  if (v)
    {
      g_variant_get (v, "(v)", &value);
      if (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
        out = g_variant_get_boolean (value);
      g_variant_unref (value);
      g_variant_unref (v);
    }

  return out;
}

/* ---- property writes -------------------------------------------------- */

static void
set_bool_prop (MskDbusWifi *self,
               const gchar *prop,
               gboolean value)
{
  if (!self->link_path)
    return;

  g_dbus_connection_call (self->connection,
                          MSK_WIFI_BUS_NAME,
                          self->link_path,
                          MSK_WIFI_PROPERTIES,
                          "Set",
                          g_variant_new ("(ssv)",
                                         MSK_WIFI_LINK_IFACE,
                                         prop,
                                         g_variant_new_boolean (value)),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1, NULL, NULL, NULL);
}

static void
set_str_prop (MskDbusWifi *self,
              const gchar *prop,
              const gchar *value)
{
  if (!self->link_path)
    return;

  g_dbus_connection_call (self->connection,
                          MSK_WIFI_BUS_NAME,
                          self->link_path,
                          MSK_WIFI_PROPERTIES,
                          "Set",
                          g_variant_new ("(ssv)",
                                         MSK_WIFI_LINK_IFACE,
                                         prop,
                                         g_variant_new_string (value)),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1, NULL, NULL, NULL);
}

/* ---- peer methods ----------------------------------------------------- */

static void
peer_call (MskDbusWifi *self, const gchar *peer_path, const gchar *method)
{
  if (!peer_path || !*peer_path)
    return;

  g_dbus_connection_call (self->connection,
                          MSK_WIFI_BUS_NAME,
                          peer_path,
                          MSK_WIFI_PEER_IFACE,
                          method,
                          NULL,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1, NULL, NULL, NULL);
}

/* ---- signal handlers -------------------------------------------------- */

static void
on_peer_go_neg (GDBusConnection *connection G_GNUC_UNUSED,
                const gchar *sender G_GNUC_UNUSED,
                const gchar *path,
                const gchar *iface G_GNUC_UNUSED,
                const gchar *signal G_GNUC_UNUSED,
                GVariant *params,
                gpointer user_data)
{
  MskDbusWifi *self = MSK_DBUS_WIFI (user_data);
  const gchar *prov, *pin;

  g_variant_get (params, "(&s&s)", &prov, &pin);
  g_signal_emit (self, signals[SIGNAL_PEER_GO_NEG], 0, path, prov, pin);
}

static void
on_peer_properties_changed (GDBusConnection *connection G_GNUC_UNUSED,
                            const gchar *sender G_GNUC_UNUSED,
                            const gchar *path,
                            const gchar *iface,
                            const gchar *signal G_GNUC_UNUSED,
                            GVariant *params,
                            gpointer user_data)
{
  MskDbusWifi *self = MSK_DBUS_WIFI (user_data);
  GVariant *changed;
  GVariantIter iter;
  const gchar *prop;
  GVariant *value;
  gboolean connected;

  g_variant_get (params, "(&s@a{sv}@as)", &iface, &changed, NULL);
  if (g_strcmp0 (iface, MSK_WIFI_PEER_IFACE) != 0)
    {
      g_variant_unref (changed);
      return;
    }

  g_variant_iter_init (&iter, changed);
  while (g_variant_iter_next (&iter, "{&sv}", &prop, &value))
    {
      if (g_strcmp0 (prop, "Connected") == 0 &&
          g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
        {
          connected = g_variant_get_boolean (value);
          if (connected)
            {
              gchar *remote = get_str_prop (self->connection, path,
                                            MSK_WIFI_PEER_IFACE,
                                            "RemoteAddress");
              emit_peer_connected (self, path, remote ? remote : "");
              g_free (remote);
            }
          else
            {
              emit_peer_disconnected (self, path);
            }
        }
      g_variant_unref (value);
    }
  g_variant_unref (changed);
}

static void
on_interfaces_added (GDBusConnection *connection G_GNUC_UNUSED,
                     const gchar *sender G_GNUC_UNUSED,
                     const gchar *path,
                     const gchar *iface G_GNUC_UNUSED,
                     const gchar *signal G_GNUC_UNUSED,
                     GVariant *params,
                     gpointer user_data)
{
  MskDbusWifi *self = MSK_DBUS_WIFI (user_data);
  GVariant *ifaces;
  GVariant *link_props;

  g_variant_get (params, "(&o@a{sa{sv}})", &path, &ifaces);

  link_props = g_variant_lookup_value (ifaces, MSK_WIFI_LINK_IFACE,
                                       G_VARIANT_TYPE ("a{sv}"));
  if (link_props)
    {
      gchar *name = NULL;
      gchar *wfd;

      g_variant_lookup (link_props, "InterfaceName", "&s", &name);
      wfd = get_str_prop (self->connection, path, MSK_WIFI_LINK_IFACE,
                          "WfdSubelements");

      if (wfd && *wfd)
        {
          g_free (self->link_path);
          self->link_path = g_strdup (path);
          g_free (self->link_name);
          self->link_name = g_strdup (name ? name : "");
          g_signal_emit (self, signals[SIGNAL_LINK_ADDED], 0, path,
                         self->link_name);
        }
      g_free (wfd);
      g_variant_unref (link_props);
    }

  g_variant_unref (ifaces);
}

/* ---- public API ------------------------------------------------------- */

static gboolean
find_link (MskDbusWifi *self)
{
  GVariant *objects, *children, *ifaces;
  GVariantIter iter;
  const gchar *path;
  gchar *name = NULL;

  objects = g_dbus_connection_call_sync (self->connection,
                                         MSK_WIFI_BUS_NAME,
                                         MSK_WIFI_PATH,
                                         MSK_WIFI_OBJECT_MANAGER,
                                         "GetManagedObjects",
                                         NULL,
                                         G_VARIANT_TYPE ("(a{oa{sa{sv}}})"),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         5000, NULL, NULL);
  if (!objects)
    return FALSE;

  children = g_variant_get_child_value (objects, 0);
  g_variant_iter_init (&iter, children);
  while (g_variant_iter_next (&iter, "o@a{sa{sv}}", &path, &ifaces))
    {
      GVariant *link_props;
      gchar *wfd;

      link_props = g_variant_lookup_value (ifaces, MSK_WIFI_LINK_IFACE,
                                           G_VARIANT_TYPE ("a{sv}"));
      if (!link_props)
        {
          g_variant_unref (ifaces);
          continue;
        }

      g_variant_lookup (link_props, "InterfaceName", "&s", &name);
      wfd = get_str_prop (self->connection, path, MSK_WIFI_LINK_IFACE,
                          "WfdSubelements");
      if (wfd && *wfd)
        {
          g_free (self->link_path);
          self->link_path = g_strdup (path);
          g_free (self->link_name);
          self->link_name = g_strdup (name ? name : "");
          g_free (wfd);
          g_variant_unref (link_props);
          g_variant_unref (ifaces);
          g_variant_unref (children);
          g_variant_unref (objects);
          return TRUE;
        }
      g_free (wfd);
      g_variant_unref (link_props);
      g_variant_unref (ifaces);
    }

  g_variant_unref (children);
  g_variant_unref (objects);
  return FALSE;
}

static void
watch_signals (MskDbusWifi *self)
{
  g_dbus_connection_signal_subscribe (self->connection,
                                      MSK_WIFI_BUS_NAME,
                                      MSK_WIFI_PEER_IFACE,
                                      "GoNegRequest",
                                      NULL, NULL,
                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                      on_peer_go_neg, self, NULL);

  g_dbus_connection_signal_subscribe (self->connection,
                                      MSK_WIFI_BUS_NAME,
                                      MSK_WIFI_PROPERTIES,
                                      "PropertiesChanged",
                                      NULL, NULL,
                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                      on_peer_properties_changed, self, NULL);

  g_dbus_connection_signal_subscribe (self->connection,
                                      MSK_WIFI_BUS_NAME,
                                      MSK_WIFI_OBJECT_MANAGER,
                                      "InterfacesAdded",
                                      NULL, NULL,
                                      G_DBUS_SIGNAL_FLAGS_NONE,
                                      on_interfaces_added, self, NULL);
}

static void
on_bus_acquired (GDBusConnection *connection, const gchar *name G_GNUC_UNUSED,
                 gpointer user_data)
{
  MskDbusWifi *self = MSK_DBUS_WIFI (user_data);

  self->connection = g_object_ref (connection);
}

MskDbusWifi *
msk_dbus_wifi_new (GError **error)
{
  MskDbusWifi *self;
  GDBusConnection *conn;
  GVariant *result;
  gboolean owner;

  conn = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, error);
  if (!conn)
    return NULL;

  result = g_dbus_connection_call_sync (conn,
                                        "org.freedesktop.DBus",
                                        "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus",
                                        "NameHasOwner",
                                        g_variant_new ("(s)", MSK_WIFI_BUS_NAME),
                                        G_VARIANT_TYPE ("(b)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        5000, NULL, error);
  if (!result)
    {
      g_object_unref (conn);
      return NULL;
    }

  g_variant_get (result, "(b)", &owner);
  g_variant_unref (result);

  if (!owner)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "miracle-wifid is not running");
      g_object_unref (conn);
      return NULL;
    }

  self = g_object_new (MSK_TYPE_DBUS_WIFI, NULL);
  self->connection = conn;

  watch_signals (self);

  if (find_link (self))
    {
      gchar *name = get_str_prop (conn, self->link_path,
                                  MSK_WIFI_LINK_IFACE, "InterfaceName");
      g_signal_emit (self, signals[SIGNAL_LINK_ADDED], 0,
                     self->link_path, name ? name : "");
      g_free (name);
    }

  return self;
}

void
msk_dbus_wifi_start (MskDbusWifi *self)
{
  if (self->running)
    return;

  self->running = TRUE;
  msk_dbus_wifi_start_scanning (self);
}

const gchar *
msk_dbus_wifi_get_link_path (MskDbusWifi *self)
{
  return self->link_path;
}

void
msk_dbus_wifi_start_scanning (MskDbusWifi *self)
{
  self->scanning = TRUE;
  set_bool_prop (self, "P2PScanning", TRUE);
}

void
msk_dbus_wifi_stop_scanning (MskDbusWifi *self)
{
  self->scanning = FALSE;
  set_bool_prop (self, "P2PScanning", FALSE);
}

void
msk_dbus_wifi_set_friendly_name (MskDbusWifi *self, const gchar *name)
{
  set_str_prop (self, "FriendlyName", name);
}

void
msk_dbus_wifi_accept_peer (MskDbusWifi *self, const gchar *peer_path)
{
  if (!peer_path || !*peer_path)
    return;

  /* prov="auto", pin="" -> accept any incoming connection */
  g_dbus_connection_call (self->connection,
                          MSK_WIFI_BUS_NAME,
                          peer_path,
                          MSK_WIFI_PEER_IFACE,
                          "Connect",
                          g_variant_new ("(ss)", "auto", ""),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1, NULL, NULL, NULL);
}

void
msk_dbus_wifi_disconnect_peer (MskDbusWifi *self, const gchar *peer_path)
{
  peer_call (self, peer_path, "Disconnect");
}

static void
msk_dbus_wifi_finalize (GObject *object)
{
  MskDbusWifi *self = MSK_DBUS_WIFI (object);

  if (self->scanning)
    set_bool_prop (self, "P2PScanning", FALSE);

  g_clear_object (&self->connection);
  g_free (self->link_path);
  g_free (self->link_name);

  G_OBJECT_CLASS (msk_dbus_wifi_parent_class)->finalize (object);
}

static void
msk_dbus_wifi_class_init (MskDbusWifiClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = msk_dbus_wifi_finalize;

  signals[SIGNAL_LINK_ADDED] =
    g_signal_new (MSK_DBUS_WIFI_SIGNAL_LINK_ADDED,
                  MSK_TYPE_DBUS_WIFI, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

  signals[SIGNAL_PEER_ADDED] =
    g_signal_new (MSK_DBUS_WIFI_SIGNAL_PEER_ADDED,
                  MSK_TYPE_DBUS_WIFI, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  signals[SIGNAL_PEER_GO_NEG] =
    g_signal_new (MSK_DBUS_WIFI_SIGNAL_PEER_GO_NEG,
                  MSK_TYPE_DBUS_WIFI, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  signals[SIGNAL_PEER_CONNECTED] =
    g_signal_new (MSK_DBUS_WIFI_SIGNAL_PEER_CONNECTED,
                  MSK_TYPE_DBUS_WIFI, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

  signals[SIGNAL_PEER_DISCONNECTED] =
    g_signal_new (MSK_DBUS_WIFI_SIGNAL_PEER_DISCONNECTED,
                  MSK_TYPE_DBUS_WIFI, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
msk_dbus_wifi_init (MskDbusWifi *self G_GNUC_UNUSED)
{
}