#include <adwaita.h>
#include <gst/gst.h>
#include "window.h"
#include "common.h"
#include "backend/msk-backend.h"
#include "backend/miracast/msk-miracast-backend.h"
#ifdef HAVE_CHROMECAST
#include "backend/chromecast/msk-chromecast-backend.h"
#endif

typedef struct
{
  MskWindow *window;
  GPtrArray *backends;
  MskBackend *active_backend;
} App;

static void set_status (App *app, const char *status);

static gboolean
is_backend_active (App *app, MskBackend *backend)
{
  return app->active_backend == backend;
}

static void
set_active_backend (App *app, MskBackend *backend)
{
  app->active_backend = backend;
}

static void
on_connected (MskBackend *backend, const char *device_name, App *app)
{
  gchar *status;

  if (is_backend_active (app, backend))
    return;

  set_active_backend (app, backend);

  status = g_strdup_printf ("Device connected (%s)", device_name);
  msk_window_set_device (app->window, device_name);
  msk_window_set_backend (app->window, msk_backend_get_name (backend));
  set_status (app, status);
  g_free (status);

  GdkPaintable *paintable = msk_backend_get_paintable (backend);
  if (paintable)
    msk_window_show_video (app->window, paintable);
  else
    msk_window_set_streaming (app->window, FALSE);
}

static void
on_disconnected (MskBackend *backend, App *app)
{
  if (!is_backend_active (app, backend))
    return;

  set_active_backend (app, NULL);
  msk_window_clear_video (app->window);
  msk_window_set_device (app->window, "");
  msk_window_set_backend (app->window, "");
  set_status (app, "Ready");
}

static void
on_status_changed (MskBackend *backend, const char *status, App *app)
{
  const char *name = msk_backend_get_name (backend);

  msk_window_set_backend_status (app->window, name, status);

  if (app->active_backend)
    {
      if (app->active_backend == backend)
        set_status (app, status);
    }
  else
    {
      set_status (app, status);
    }
}

static void
on_disconnect_requested (MskWindow *window G_GNUC_UNUSED, App *app)
{
  guint i;

  for (i = 0; i < app->backends->len; i++)
    {
      MskBackend *b = g_ptr_array_index (app->backends, i);
      msk_backend_stop (b);
    }

  set_active_backend (app, NULL);
  msk_window_clear_video (app->window);
  msk_window_set_device (app->window, "");
  msk_window_set_backend (app->window, "");
  set_status (app, "Ready");
}

static void
connect_backend (App *app, MskBackend *backend)
{
  g_signal_connect (backend, "connected",
                    G_CALLBACK (on_connected), app);
  g_signal_connect (backend, "disconnected",
                    G_CALLBACK (on_disconnected), app);
  g_signal_connect (backend, "status-changed",
                    G_CALLBACK (on_status_changed), app);
}

static void
on_window_destroy (GtkWindow *window G_GNUC_UNUSED, App *app)
{
  guint i;

  for (i = 0; i < app->backends->len; i++)
    msk_backend_stop (g_ptr_array_index (app->backends, i));

  g_ptr_array_unref (app->backends);
  g_free (app);
}

static void
activate (GtkApplication *gtk_app G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
  App *app = g_new0 (App, 1);

  app->backends = g_ptr_array_new_with_free_func (g_object_unref);
  app->window = msk_window_new (gtk_app);

  MskBackend *miracast = msk_miracast_backend_new ();
  g_ptr_array_add (app->backends, miracast);
  connect_backend (app, miracast);
  msk_backend_start (miracast);
  msk_window_set_backend_status (app->window, "Miracast", "Starting…");

#ifdef HAVE_CHROMECAST
  MskBackend *chromecast = msk_chromecast_backend_new ();
  g_ptr_array_add (app->backends, chromecast);
  connect_backend (app, chromecast);
  msk_backend_start (chromecast);
  msk_window_set_backend_status (app->window, "Chromecast", "Starting…");
#endif

  g_signal_connect (app->window, "disconnect-requested",
                    G_CALLBACK (on_disconnect_requested), app);
  g_signal_connect (app->window, "destroy",
                    G_CALLBACK (on_window_destroy), app);

  gtk_window_present (GTK_WINDOW (app->window));
}

static void
set_status (App *app, const char *status)
{
  msk_window_set_status (app->window, status);
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
