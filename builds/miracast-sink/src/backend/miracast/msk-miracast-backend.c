#include "msk-miracast-backend.h"
#include "dbus-wifi.h"
#include "rtsp-wfd.h"
#include "../../common.h"
#include "../../stream.h"
#include "../../window.h"

struct _MskMiracastBackend
{
  GObject parent_instance;

  MskDbusWifi *wifi;
  MskRtspWfd *rtsp;
  MskStream *stream;
  MskWindow *window;
  gchar *peer_path;
};

enum
{
  SIGNAL_CONNECTED,
  SIGNAL_DISCONNECTED,
  SIGNAL_STATUS_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void msk_miracast_backend_interface_init (MskBackendInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MskMiracastBackend, msk_miracast_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MSK_TYPE_BACKEND,
                                                msk_miracast_backend_interface_init))

static void
set_status (MskMiracastBackend *self, const char *status)
{
  g_signal_emit (self, signals[SIGNAL_STATUS_CHANGED], 0, status);
}

static void
start_stream (MskMiracastBackend *self, const char *peer_address)
{
  msk_stream_start (self->stream);
  msk_rtsp_wfd_connect_peer (self->rtsp, peer_address);
  set_status (self, "Streaming");
}

static void
stop_stream (MskMiracastBackend *self)
{
  msk_stream_stop (self->stream);
  msk_rtsp_wfd_stop (self->rtsp);
  set_status (self, "Ready");
}

static void
on_link_added (MskDbusWifi *wifi, const char *path, const char *name,
               MskMiracastBackend *self)
{
  g_debug ("link added: %s (%s)", path, name);
  set_status (self, "Listening for devices…");
  msk_dbus_wifi_start (wifi);
}

static void
on_peer_go_neg (MskDbusWifi *wifi, const char *peer_path,
                const char *prov, const char *pin, MskMiracastBackend *self)
{
  g_debug ("GO-NEG from %s (%s/%s), auto-accepting", peer_path, prov, pin);
  msk_dbus_wifi_accept_peer (wifi, peer_path);
}

static void
on_peer_connected (MskDbusWifi *wifi, const char *peer_path,
                   const char *remote_address, MskMiracastBackend *self)
{
  gchar *status;

  g_free (self->peer_path);
  self->peer_path = g_strdup (peer_path);

  status = g_strdup_printf ("Device connected (%s)", remote_address);
  g_signal_emit (self, signals[SIGNAL_CONNECTED], 0, remote_address);
  set_status (self, status);
  g_free (status);

  start_stream (self, remote_address);
}

static void
on_peer_disconnected (MskDbusWifi *wifi, const char *peer_path,
                      MskMiracastBackend *self)
{
  stop_stream (self);
  g_signal_emit (self, signals[SIGNAL_DISCONNECTED], 0);
}

static void
on_stream_ready (MskRtspWfd *rtsp, MskMiracastBackend *self)
{
  set_status (self, "Streaming");
}

static void
msk_miracast_backend_start (MskBackend *backend)
{
  MskMiracastBackend *self = MSK_MIRACAST_BACKEND (backend);
  GError *error = NULL;

  self->stream = msk_stream_new (MSK_DEFAULT_PORT);
  self->rtsp = msk_rtsp_wfd_new (MSK_DEFAULT_PORT, &error);

  if (error)
    {
      g_warning ("RTSP init failed: %s", error->message);
      g_error_free (error);
    }

  self->wifi = msk_dbus_wifi_new (&error);
  if (error)
    {
      g_warning ("D-Bus: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (self->wifi, MSK_DBUS_WIFI_SIGNAL_LINK_ADDED,
                    G_CALLBACK (on_link_added), self);
  g_signal_connect (self->wifi, MSK_DBUS_WIFI_SIGNAL_PEER_GO_NEG,
                    G_CALLBACK (on_peer_go_neg), self);
  g_signal_connect (self->wifi, MSK_DBUS_WIFI_SIGNAL_PEER_CONNECTED,
                    G_CALLBACK (on_peer_connected), self);
  g_signal_connect (self->wifi, MSK_DBUS_WIFI_SIGNAL_PEER_DISCONNECTED,
                    G_CALLBACK (on_peer_disconnected), self);
  g_signal_connect (self->rtsp, MSK_RTSP_WFD_SIGNAL_STREAM_READY,
                    G_CALLBACK (on_stream_ready), self);

  set_status (self, "Ready");
}

static void
msk_miracast_backend_stop (MskBackend *backend)
{
  MskMiracastBackend *self = MSK_MIRACAST_BACKEND (backend);

  stop_stream (self);

  if (self->peer_path)
    msk_dbus_wifi_disconnect_peer (self->wifi, self->peer_path);

  g_clear_object (&self->wifi);
  g_clear_object (&self->rtsp);
  g_clear_object (&self->stream);
  g_clear_pointer (&self->peer_path, g_free);
}

static GdkPaintable *
msk_miracast_backend_get_paintable (MskBackend *backend)
{
  MskMiracastBackend *self = MSK_MIRACAST_BACKEND (backend);

  if (self->stream)
    return msk_stream_get_paintable (self->stream);

  return NULL;
}

static const char *
msk_miracast_backend_get_name (MskBackend *backend)
{
  return "Miracast";
}

static void
msk_miracast_backend_interface_init (MskBackendInterface *iface)
{
  iface->start = msk_miracast_backend_start;
  iface->stop = msk_miracast_backend_stop;
  iface->get_paintable = msk_miracast_backend_get_paintable;
  iface->get_name = msk_miracast_backend_get_name;
}

static void
msk_miracast_backend_finalize (GObject *object)
{
  MskMiracastBackend *self = MSK_MIRACAST_BACKEND (object);

  g_clear_object (&self->wifi);
  g_clear_object (&self->rtsp);
  g_clear_object (&self->stream);
  g_clear_pointer (&self->peer_path, g_free);

  G_OBJECT_CLASS (msk_miracast_backend_parent_class)->finalize (object);
}

static void
msk_miracast_backend_class_init (MskMiracastBackendClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = msk_miracast_backend_finalize;

  signals[SIGNAL_CONNECTED] =
    g_signal_new ("connected",
                  MSK_TYPE_MIRACAST_BACKEND, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  signals[SIGNAL_DISCONNECTED] =
    g_signal_new ("disconnected",
                  MSK_TYPE_MIRACAST_BACKEND, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[SIGNAL_STATUS_CHANGED] =
    g_signal_new ("status-changed",
                  MSK_TYPE_MIRACAST_BACKEND, G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
msk_miracast_backend_init (MskMiracastBackend *self)
{
}

MskBackend *
msk_miracast_backend_new (void)
{
  return g_object_new (MSK_TYPE_MIRACAST_BACKEND, NULL);
}
