#include "discovery.h"

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-glib/glib-malloc.h>
#include <avahi-glib/glib-watch.h>

struct _MskChromecastDiscovery
{
  GObject parent_instance;

  AvahiClient *client;
  AvahiGLibPoll *glib_poll;
  AvahiServiceBrowser *browser;

  GList *devices;
};

G_DEFINE_TYPE (MskChromecastDiscovery, msk_chromecast_discovery, G_TYPE_OBJECT)

enum
{
  SIGNAL_DEVICE_FOUND,
  SIGNAL_DEVICE_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

void
msk_chromecast_device_free (MskChromecastDevice *device)
{
  if (device)
    {
      g_free (device->name);
      g_free (device->host);
      g_free (device->id);
      g_free (device->model);
      g_free (device);
    }
}

static MskChromecastDevice *
find_device_by_name (MskChromecastDiscovery *self, const gchar *name)
{
  GList *l;

  for (l = self->devices; l; l = l->next)
    {
      MskChromecastDevice *device = l->data;
      if (g_strcmp0 (device->name, name) == 0)
        return device;
    }

  return NULL;
}

static void
resolve_callback (
  AvahiServiceResolver *resolver G_GNUC_UNUSED,
  G_GNUC_UNUSED AvahiIfIndex interface,
  G_GNUC_UNUSED AvahiProtocol protocol,
  AvahiResolverEvent event,
  const char *name,
  const char *type G_GNUC_UNUSED,
  const char *domain G_GNUC_UNUSED,
  const char *host_name G_GNUC_UNUSED,
  const AvahiAddress *address,
  uint16_t port,
  AvahiStringList *txt,
  G_GNUC_UNUSED AvahiLookupResultFlags flags,
  gpointer userdata)
{
  MskChromecastDiscovery *self = MSK_CHROMECAST_DISCOVERY (userdata);
  char addr[AVAHI_ADDRESS_STR_MAX];

  if (event != AVAHI_RESOLVER_FOUND)
    return;

  if (!find_device_by_name (self, name))
    {
      MskChromecastDevice *device;
      AvahiStringList *entry;
      gchar *id = NULL;
      gchar *model = NULL;

      for (entry = txt; entry; entry = entry->next)
        {
          gchar *key;
          gchar *value;
          gsize length;

          if (avahi_string_list_get_pair (entry, &key, &value, &length) == 0)
            {
              if (g_strcmp0 (key, "id") == 0)
                id = g_strndup (value, length);
              else if (g_strcmp0 (key, "md") == 0)
                model = g_strndup (value, length);
              avahi_free (key);
              avahi_free (value);
            }
        }

      avahi_address_snprint (addr, sizeof (addr), address);

      device = g_new0 (MskChromecastDevice, 1);
      device->name = g_strdup (name);
      device->host = g_strdup (addr);
      device->port = port;
      device->id = id ? id : g_strdup ("");
      device->model = model ? model : g_strdup ("");

      self->devices = g_list_append (self->devices, device);

      g_signal_emit (self, signals[SIGNAL_DEVICE_FOUND], 0, device);
    }
}

static void
browse_callback (
  AvahiServiceBrowser *browser G_GNUC_UNUSED,
  AvahiIfIndex interface,
  AvahiProtocol protocol,
  AvahiBrowserEvent event,
  const char *name,
  const char *type G_GNUC_UNUSED,
  const char *domain G_GNUC_UNUSED,
  G_GNUC_UNUSED AvahiLookupResultFlags flags,
  gpointer userdata)
{
  MskChromecastDiscovery *self = MSK_CHROMECAST_DISCOVERY (userdata);
  AvahiServiceResolver *resolver;

  switch (event)
    {
    case AVAHI_BROWSER_FAILURE:
      g_warning ("Avahi browse failure");
      break;

    case AVAHI_BROWSER_NEW:
      resolver = avahi_service_resolver_new (self->client,
                                             interface, protocol,
                                             name, "_googlecast._tcp", domain,
                                             AVAHI_IF_UNSPEC, 0,
                                             resolve_callback, self);
      if (resolver)
        avahi_service_resolver_free (resolver);
      break;

    case AVAHI_BROWSER_REMOVE:
      {
        GList *l;

        for (l = self->devices; l; l = l->next)
          {
            MskChromecastDevice *device = l->data;
            if (g_strcmp0 (device->name, name) == 0)
              {
                g_signal_emit (self, signals[SIGNAL_DEVICE_REMOVED], 0, device);
                self->devices = g_list_delete_link (self->devices, l);
                msk_chromecast_device_free (device);
                break;
              }
          }
      }
      break;

    default:
      break;
    }
}

static void
client_callback (AvahiClient *client, AvahiClientState state, gpointer userdata)
{
  MskChromecastDiscovery *self = MSK_CHROMECAST_DISCOVERY (userdata);

  switch (state)
    {
    case AVAHI_CLIENT_S_RUNNING:
      self->browser = avahi_service_browser_new (client,
                                                 AVAHI_IF_UNSPEC,
                                                 AVAHI_PROTOCOL_UNSPEC,
                                                 "_googlecast._tcp",
                                                 NULL, 0,
                                                 browse_callback, self);
      break;

    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_S_REGISTERING:
      if (self->browser)
        {
          avahi_service_browser_free (self->browser);
          self->browser = NULL;
        }
      break;

    case AVAHI_CLIENT_CONNECTING:
    case AVAHI_CLIENT_FAILURE:
      if (self->browser)
        {
          avahi_service_browser_free (self->browser);
          self->browser = NULL;
        }
      break;
    }
}

static void
msk_chromecast_discovery_finalize (GObject *object)
{
  MskChromecastDiscovery *self = MSK_CHROMECAST_DISCOVERY (object);

  msk_chromecast_discovery_stop (self);
  g_list_free_full (self->devices, (GDestroyNotify) msk_chromecast_device_free);
  self->devices = NULL;

  G_OBJECT_CLASS (msk_chromecast_discovery_parent_class)->finalize (object);
}

static void
msk_chromecast_discovery_class_init (MskChromecastDiscoveryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = msk_chromecast_discovery_finalize;

  signals[SIGNAL_DEVICE_FOUND] =
    g_signal_new ("device-found",
                  MSK_TYPE_CHROMECAST_DISCOVERY, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  signals[SIGNAL_DEVICE_REMOVED] =
    g_signal_new ("device-removed",
                  MSK_TYPE_CHROMECAST_DISCOVERY, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
msk_chromecast_discovery_init (MskChromecastDiscovery *self G_GNUC_UNUSED)
{
}

MskChromecastDiscovery *
msk_chromecast_discovery_new (void)
{
  return g_object_new (MSK_TYPE_CHROMECAST_DISCOVERY, NULL);
}

void
msk_chromecast_discovery_start (MskChromecastDiscovery *self)
{
  int error;

  if (self->client)
    return;

  self->glib_poll = avahi_glib_poll_new (NULL, NULL);
  self->client = avahi_client_new (avahi_glib_poll_get (self->glib_poll),
                                   0, client_callback, self, &error);

  if (!self->client)
    {
      g_warning ("Avahi client failed: error %d", error);
      avahi_glib_poll_free (self->glib_poll);
      self->glib_poll = NULL;
    }
}

void
msk_chromecast_discovery_stop (MskChromecastDiscovery *self)
{
  if (self->browser)
    {
      avahi_service_browser_free (self->browser);
      self->browser = NULL;
    }

  if (self->client)
    {
      avahi_client_free (self->client);
      self->client = NULL;
    }

  if (self->glib_poll)
    {
      avahi_glib_poll_free (self->glib_poll);
      self->glib_poll = NULL;
    }
}
