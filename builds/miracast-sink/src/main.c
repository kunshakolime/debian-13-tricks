#include <adwaita.h>
#include <gst/gst.h>
#include "window.h"
#include "common.h"
#include "backend/msk-backend.h"
#include "backend/miracast/msk-miracast-backend.h"

typedef struct
{
  MskWindow *window;
  MskBackend *backend;
} App;

static void
set_status (App *app, const char *status)
{
  msk_window_set_status (app->window, status);
}

static void
on_connected (MskBackend *backend, const char *device_name, App *app)
{
  gchar *status;

  status = g_strdup_printf ("Device connected (%s)", device_name);
  msk_window_set_device (app->window, device_name);
  set_status (app, status);
  g_free (status);

  msk_window_show_video (app->window, msk_backend_get_paintable (app->backend));
}

static void
on_disconnected (MskBackend *backend, App *app)
{
  msk_window_clear_video (app->window);
  msk_window_set_device (app->window, "");
  set_status (app, "Ready");
}

static void
on_status_changed (MskBackend *backend, const char *status, App *app)
{
  set_status (app, status);
}

static void
on_disconnect_requested (MskWindow *window, App *app)
{
  msk_backend_stop (app->backend);
  msk_window_clear_video (app->window);
  set_status (app, "Ready");
}

static void
activate (GtkApplication *gtk_app, gpointer user_data)
{
  App *app = g_new0 (App, 1);

  app->window = msk_window_new (gtk_app);

  /* TODO: Select backend based on command line or config */
  app->backend = msk_miracast_backend_new ();

  g_signal_connect (app->backend, "connected",
                    G_CALLBACK (on_connected), app);
  g_signal_connect (app->backend, "disconnected",
                    G_CALLBACK (on_disconnected), app);
  g_signal_connect (app->backend, "status-changed",
                    G_CALLBACK (on_status_changed), app);
  g_signal_connect (app->window, "disconnect-requested",
                    G_CALLBACK (on_disconnect_requested), app);

  msk_backend_start (app->backend);

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
