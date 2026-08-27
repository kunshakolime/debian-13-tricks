#include "msk-chromecast-backend.h"
#include "discovery.h"
#include "client.h"
#include "media.h"

struct _MskChromecastBackend
{
  GObject parent_instance;

  MskChromecastDiscovery *discovery;
  MskChromecastClient *client;
  MskChromecastMedia *media;

  MskChromecastDevice *current_device;
};

enum
{
  SIGNAL_CONNECTED,
  SIGNAL_DISCONNECTED,
  SIGNAL_STATUS_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

MSK_BACKEND_SIGNAL_DEFS (signals, MSK_TYPE_CHROMECAST_BACKEND)

static void msk_chromecast_backend_interface_init (MskBackendInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MskChromecastBackend, msk_chromecast_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MSK_TYPE_BACKEND,
                                                msk_chromecast_backend_interface_init))

static void
on_device_found (MskChromecastDiscovery *discovery G_GNUC_UNUSED,
                 MskChromecastDevice *device,
                 MskChromecastBackend *self)
{
  gchar *status;

  g_debug ("Chromecast found: %s at %s:%u", device->name, device->host, device->port);

  if (!self->current_device)
    {
      self->current_device = device;

      status = g_strdup_printf ("Connecting to %s...", device->name);
      msk_emit_status (self, status);
      g_free (status);

      msk_chromecast_client_connect (self->client, device->host, device->port, NULL);
    }
}

static void
on_device_removed (MskChromecastDiscovery *discovery G_GNUC_UNUSED,
                   MskChromecastDevice *device G_GNUC_UNUSED,
                   MskChromecastBackend *self)
{
  if (self->current_device == device)
    {
      self->current_device = NULL;
      msk_emit_status (self, "Device disconnected");
    }
}

static void
on_client_connected (MskChromecastClient *client, MskChromecastBackend *self)
{
  gchar *status;

  g_debug ("Connected to Chromecast");

  msk_chromecast_client_send_connect (client, "receiver-0", NULL);
  msk_chromecast_client_send_get_status (client, NULL);

  status = g_strdup_printf ("Connected to %s", self->current_device->name);
  msk_emit_connected (self, self->current_device->name);
  msk_emit_status (self, status);
  g_free (status);
}

static void
on_client_disconnected (MskChromecastClient *client G_GNUC_UNUSED, MskChromecastBackend *self)
{
  msk_emit_disconnected (self);
  msk_emit_status (self, "Disconnected");
}

static void
on_client_message (MskChromecastClient *client G_GNUC_UNUSED,
                   const gchar *namespace,
                   const gchar *payload,
                   MskChromecastBackend *self)
{
  g_debug ("Chromecast message [%s]: %s", namespace, payload);

  if (g_strcmp0 (namespace, "urn:x-cast:com.google.cast.receiver") == 0)
    {
      msk_chromecast_media_update_status (self->media, payload);
    }
  else if (g_strcmp0 (namespace, "urn:x-cast:com.google.cast.media") == 0)
    {
      msk_chromecast_media_update_status (self->media, payload);
    }
}

static void
msk_chromecast_backend_start (MskBackend *backend)
{
  MskChromecastBackend *self = MSK_CHROMECAST_BACKEND (backend);

  self->discovery = msk_chromecast_discovery_new ();
  self->client = msk_chromecast_client_new ();
  self->media = msk_chromecast_media_new (self->client);

  g_signal_connect (self->discovery, "device-found",
                    G_CALLBACK (on_device_found), self);
  g_signal_connect (self->discovery, "device-removed",
                    G_CALLBACK (on_device_removed), self);
  g_signal_connect (self->client, "connected",
                    G_CALLBACK (on_client_connected), self);
  g_signal_connect (self->client, "disconnected",
                    G_CALLBACK (on_client_disconnected), self);
  g_signal_connect (self->client, "message",
                    G_CALLBACK (on_client_message), self);

  msk_emit_status (self, "Searching for Chromecast devices...");
  msk_chromecast_discovery_start (self->discovery);
}

static void
msk_chromecast_backend_stop (MskBackend *backend)
{
  MskChromecastBackend *self = MSK_CHROMECAST_BACKEND (backend);

  msk_chromecast_discovery_stop (self->discovery);
  msk_chromecast_client_disconnect (self->client);

  g_clear_object (&self->media);
  g_clear_object (&self->client);
  g_clear_object (&self->discovery);
  self->current_device = NULL;
}

static GdkPaintable *
msk_chromecast_backend_get_paintable (MskBackend *backend G_GNUC_UNUSED)
{
  return NULL;
}

static const char *
msk_chromecast_backend_get_name (MskBackend *backend G_GNUC_UNUSED)
{
  return "Chromecast";
}

static void
msk_chromecast_backend_interface_init (MskBackendInterface *iface)
{
  iface->start = msk_chromecast_backend_start;
  iface->stop = msk_chromecast_backend_stop;
  iface->get_paintable = msk_chromecast_backend_get_paintable;
  iface->get_name = msk_chromecast_backend_get_name;
}

static void
msk_chromecast_backend_finalize (GObject *object)
{
  MskChromecastBackend *self = MSK_CHROMECAST_BACKEND (object);

  msk_chromecast_backend_stop (MSK_BACKEND (self));

  G_OBJECT_CLASS (msk_chromecast_backend_parent_class)->finalize (object);
}

static void
msk_chromecast_backend_class_init (MskChromecastBackendClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = msk_chromecast_backend_finalize;

  msk_register_signals ();
}

static void
msk_chromecast_backend_init (MskChromecastBackend *self G_GNUC_UNUSED)
{
}

MskBackend *
msk_chromecast_backend_new (void)
{
  return g_object_new (MSK_TYPE_CHROMECAST_BACKEND, NULL);
}
