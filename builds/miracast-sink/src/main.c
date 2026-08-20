#include <adwaita.h>
#include <gst/gst.h>
#include "window.h"
#include "dbus-wifi.h"
#include "rtsp-wfd.h"
#include "stream.h"
#include "common.h"

typedef struct
{
  MskWindow *window;
  MskDbusWifi *wifi;
  MskRtspWfd *rtsp;
  MskStream *stream;
  gchar *peer_path;
} App;

static void
set_status (App *app, const char *status)
{
  msk_window_set_status (app->window, status);
}

static void
start_stream (App *app, const char *peer_address)
{
  msk_stream_start (app->stream);
  msk_rtsp_wfd_connect_peer (app->rtsp, peer_address);
  msk_window_show_video (app->window,
                         msk_stream_get_paintable (app->stream));
  set_status (app, "Streaming");
}

static void
stop_stream (App *app)
{
  msk_stream_stop (app->stream);
  msk_rtsp_wfd_stop (app->rtsp);
  msk_window_clear_video (app->window);
  set_status (app, "Ready");
}

static void
on_link_added (MskDbusWifi *wifi, const char *path, const char *name,
               App *app)
{
  g_debug ("link added: %s (%s)", path, name);
  set_status (app, "Listening for devices…");
  msk_dbus_wifi_start (wifi);
}

static void
on_peer_go_neg (MskDbusWifi *wifi, const char *peer_path,
                const char *prov, const char *pin, App *app)
{
  g_debug ("GO-NEG from %s (%s/%s), auto-accepting", peer_path, prov, pin);
  msk_dbus_wifi_accept_peer (wifi, peer_path);
}

static void
on_peer_connected (MskDbusWifi *wifi, const char *peer_path,
                   const char *remote_address, App *app)
{
  gchar *status;

  g_free (app->peer_path);
  app->peer_path = g_strdup (peer_path);

  status = g_strdup_printf ("Device connected (%s)", remote_address);
  msk_window_set_device (app->window, remote_address);
  set_status (app, status);
  g_free (status);

  start_stream (app, remote_address);
}

static void
on_peer_disconnected (MskDbusWifi *wifi, const char *peer_path, App *app)
{
  stop_stream (app);
  msk_window_set_device (app->window, "");
}

static void
on_stream_ready (MskRtspWfd *rtsp, App *app)
{
  set_status (app, "Streaming");
}

static void
on_disconnect_requested (MskWindow *window, App *app)
{
  if (app->peer_path)
    msk_dbus_wifi_disconnect_peer (app->wifi, app->peer_path);
  stop_stream (app);
}

static void
activate (GtkApplication *gtk_app, gpointer user_data)
{
  App *app = g_new0 (App, 1);
  GError *error = NULL;

  app->window = msk_window_new (gtk_app);
  app->stream = msk_stream_new (MSK_DEFAULT_PORT);
  app->rtsp = msk_rtsp_wfd_new (MSK_DEFAULT_PORT, &error);

  if (error)
    {
      g_warning ("RTSP init failed: %s", error->message);
      g_error_free (error);
    }

  app->wifi = msk_dbus_wifi_new (&error);
  if (error)
    {
      AdwDialog *dialog;

      g_warning ("D-Bus: %s", error->message);

      dialog = adw_alert_dialog_new ("MiracleCast not running",
                                     "Could not connect to miracle-wifid. "
                                     "Start it with: "
                                     "sudo systemctl start miracast-wifid");
      adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "ok", "_OK");
      adw_dialog_present (dialog, GTK_WIDGET (app->window));
      g_error_free (error);
      return;
    }

  g_signal_connect (app->wifi, MSK_DBUS_WIFI_SIGNAL_LINK_ADDED,
                    G_CALLBACK (on_link_added), app);
  g_signal_connect (app->wifi, MSK_DBUS_WIFI_SIGNAL_PEER_GO_NEG,
                    G_CALLBACK (on_peer_go_neg), app);
  g_signal_connect (app->wifi, MSK_DBUS_WIFI_SIGNAL_PEER_CONNECTED,
                    G_CALLBACK (on_peer_connected), app);
  g_signal_connect (app->wifi, MSK_DBUS_WIFI_SIGNAL_PEER_DISCONNECTED,
                    G_CALLBACK (on_peer_disconnected), app);
  g_signal_connect (app->rtsp, MSK_RTSP_WFD_SIGNAL_STREAM_READY,
                    G_CALLBACK (on_stream_ready), app);
  g_signal_connect (app->window, "disconnect-requested",
                    G_CALLBACK (on_disconnect_requested), app);

  set_status (app, "Ready");

  gtk_window_present (GTK_WINDOW (app->window));
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  int status;

  gst_init (&argc, &argv);

  app = gtk_application_new (MSK_APP_ID, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}