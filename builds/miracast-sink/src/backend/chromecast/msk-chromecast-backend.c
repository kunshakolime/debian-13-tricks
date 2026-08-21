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

static void msk_chromecast_backend_interface_init (MskBackendInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MskChromecastBackend, msk_chromecast_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MSK_TYPE_BACKEND,
                                                msk_chromecast_backend_interface_init))

static void
set_status (MskChromecastBackend *self, const char *status)
{
  g_signal_emit (self, signals[SIGNAL_STATUS_CHANGED], 0, status);
}

static void
on_device_found (MskChromecastDiscovery *discovery G_GNUC_UNUSED,
                 MskChromecastDevice *device,
                 MskChromecastBackend *self)
{
  gchar *status;

  g_debug ("Chromecast found: %s at %s:%u", device->name, device->host, device->port);

  /* Auto-connect to first device found */
  if (!self->current_device)
    {
      self->current_device = device;

      status = g_strdup_printf ("Connecting to %s...", device->name);
      set_status (self, status);
      g_free (status);

      msk_chromecast_client_connect (self->client, device->host, device->port, NULL);
    }
}

static void
on_device_removed (MskChromecastDiscovery *discovery G_GNUC_UNUSED,
                   MskChromecastDevice *device,
                   MskChromecastBackend *self)
{
  if (self->current_device == device)
    {
      self->current_device = NULL;
      set_status (self, "Device disconnected");
    }
}

static void
on_client_connected (MskChromecastClient *client, MskChromecastBackend *self)
{
  gchar *status;

  g_debug ("Connected to Chromecast");

  /* Send CONNECT to receiver */
  msk_chromecast_client_send_connect (client, "receiver-0", NULL);

  /* Get status */
  msk_chromecast_client_send_get_status (client, NULL);

  status = g_strdup_printf ("Connected to %s", self->current_device->name);
  g_signal_emit (self, signals[SIGNAL_CONNECTED], 0, self->current_device->name);
  set_status (self, status);
  g_free (status);
}

static void
on_client_disconnected (MskChromecastClient *client G_GNUC_UNUSED, MskChromecastBackend *self)
{
  g_signal_emit (self, signals[SIGNAL_DISCONNECTED], 0);
  set_status (self, "Disconnected");
}

static void
on_client_message (MskChromecastClient *client G_GNUC_UNUSED, const gchar *message G_GNUC_UNUSED,
                   MskChromecastBackend *self G_GNUC_UNUSED)
{
  /* Parse JSON message and handle status updates */
  g_debug ("Chromecast message: %s", message);

  /* TODO: Parse RECEIVER_STATUS to find transportId for media namespace */
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

  set_status (self, "Searching for Chromecast devices...");
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
  /* Chromecast doesn't provide a local paintable - it renders on the TV */
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

  signals[SIGNAL_CONNECTED] =
    g_signal_new ("connected",
                  MSK_TYPE_CHROMECAST_BACKEND, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  signals[SIGNAL_DISCONNECTED] =
    g_signal_new ("disconnected",
                  MSK_TYPE_CHROMECAST_BACKEND, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[SIGNAL_STATUS_CHANGED] =
    g_signal_new ("status-changed",
                  MSK_TYPE_CHROMECAST_BACKEND, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
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
