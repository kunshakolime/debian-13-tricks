#include <adwaita.h>
#include <gst/gst.h>
#include "window.h"
#include "common.h"
#include "backend/msk-backend.h"
#include "backend/miracast/msk-miracast-backend.h"
#include "backend/chromecast/msk-chromecast-backend.h"

typedef struct
{
  MskWindow *window;
  GPtrArray *backends;
  MskBackend *active_backend;
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

  app->active_backend = backend;

  status = g_strdup_printf ("Device connected (%s)", device_name);
  msk_window_set_device (app->window, device_name);
  set_status (app, status);
  g_free (status);

  GdkPaintable *paintable = msk_backend_get_paintable (backend);
  if (paintable)
    msk_window_show_video (app->window, paintable);
}

static void
on_disconnected (MskBackend *backend G_GNUC_UNUSED, App *app)
{
  app->active_backend = NULL;
  msk_window_clear_video (app->window);
  msk_window_set_device (app->window, "");
  set_status (app, "Ready");
}

static void
on_status_changed (MskBackend *backend G_GNUC_UNUSED, const char *status, App *app)
{
  set_status (app, status);
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

  app->active_backend = NULL;
  msk_window_clear_video (app->window);
  set_status (app, "Ready");
}

static void
activate (GtkApplication *gtk_app G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
  App *app = g_new0 (App, 1);

  app->backends = g_ptr_array_new_with_free_func (g_object_unref);
  app->window = msk_window_new (gtk_app);

  gboolean use_chromecast = GPOINTER_TO_INT (
    g_object_get_data (G_OBJECT (gtk_app), "chromecast"));

  if (use_chromecast)
    {
      MskBackend *cc = msk_chromecast_backend_new ();
      g_ptr_array_add (app->backends, cc);
      g_signal_connect (cc, "connected",
                        G_CALLBACK (on_connected), app);
      g_signal_connect (cc, "disconnected",
                        G_CALLBACK (on_disconnected), app);
      g_signal_connect (cc, "status-changed",
                        G_CALLBACK (on_status_changed), app);
      msk_backend_start (cc);
    }
  else
    {
      MskBackend *mc = msk_miracast_backend_new ();
      g_ptr_array_add (app->backends, mc);
      g_signal_connect (mc, "connected",
                        G_CALLBACK (on_connected), app);
      g_signal_connect (mc, "disconnected",
                        G_CALLBACK (on_disconnected), app);
      g_signal_connect (mc, "status-changed",
                        G_CALLBACK (on_status_changed), app);
      msk_backend_start (mc);
    }

  g_signal_connect (app->window, "disconnect-requested",
                    G_CALLBACK (on_disconnect_requested), app);

  gtk_window_present (GTK_WINDOW (app->window));
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  int status;
  gboolean chromecast = FALSE;

  GOptionEntry entries[] = {
    { "chromecast", 0, 0, G_OPTION_ARG_NONE, &chromecast,
      "Use Chromecast backend instead of Miracast", NULL },
    { NULL }
  };

  GOptionContext *ctx = g_option_context_new ("- Miracast/Chromecast sink");
  g_option_context_add_main_entries (ctx, entries, NULL);

  gst_init (&argc, &argv);

  app = gtk_application_new (MSK_APP_ID, G_APPLICATION_DEFAULT_FLAGS);
  g_object_set_data (G_OBJECT (app), "chromecast", GINT_TO_POINTER (chromecast));
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  g_option_context_free (ctx);

  return status;
}
