#include "window.h"
#include "common.h"

struct _MskWindow
{
  AdwApplicationWindow parent_instance;

  GtkStack *stack;
  GtkLabel *status_label;
  GtkLabel *device_label;
  GtkButton *disconnect_button;
  GtkPicture *video_picture;
};

G_DEFINE_TYPE (MskWindow, msk_window, ADW_TYPE_APPLICATION_WINDOW)

enum
{
  PROP_0,
  PROP_DISCONNECT,
  LAST_PROP,
};

static void
on_disconnect_clicked (GtkButton *button, gpointer user_data)
{
  MskWindow *self = MSK_WINDOW (user_data);

  g_signal_emit_by_name (self, "disconnect-requested");
}

static void
msk_window_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  MskWindow *self = MSK_WINDOW (object);

  switch (prop_id)
    {
    case PROP_DISCONNECT:
      if (g_value_get_boolean (value))
        g_signal_emit_by_name (self, "disconnect-requested");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
msk_window_init (MskWindow *self)
{
  AdwToolbarView *toolbar;
  AdwHeaderBar *header;
  GtkBox *box;
  GtkWidget *content;
  GtkWidget *viewport;
  GtkWidget *spinner;

  toolbar = ADW_TOOLBAR_VIEW (adw_toolbar_view_new ());
  header = ADW_HEADER_BAR (adw_header_bar_new ());
  adw_header_bar_set_title_widget (header, adw_window_title_new ("Miracast Sink", NULL));
  adw_toolbar_view_add_top_bar (toolbar, GTK_WIDGET (header));

  self->disconnect_button = GTK_BUTTON (gtk_button_new_with_label ("Disconnect"));
  gtk_widget_set_halign (GTK_WIDGET (self->disconnect_button), GTK_ALIGN_END);
  gtk_widget_set_valign (GTK_WIDGET (self->disconnect_button), GTK_ALIGN_CENTER);
  g_signal_connect (self->disconnect_button, "clicked",
                    G_CALLBACK (on_disconnect_clicked), self);
  adw_header_bar_pack_end (header, GTK_WIDGET (self->disconnect_button));

  box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 12));
  gtk_widget_set_margin_top (GTK_WIDGET (box), 24);
  gtk_widget_set_margin_bottom (GTK_WIDGET (box), 24);
  gtk_widget_set_margin_start (GTK_WIDGET (box), 24);
  gtk_widget_set_margin_end (GTK_WIDGET (box), 24);

  self->status_label = GTK_LABEL (gtk_label_new ("Starting…"));
  gtk_label_set_wrap (self->status_label, TRUE);
  gtk_widget_add_css_class (GTK_WIDGET (self->status_label), "title-3");

  self->device_label = GTK_LABEL (gtk_label_new (""));
  gtk_label_set_wrap (self->device_label, TRUE);

  self->video_picture = GTK_PICTURE (gtk_picture_new ());
  gtk_picture_set_can_shrink (self->video_picture, TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self->video_picture), TRUE);

  spinner = gtk_spinner_new ();
  gtk_widget_set_size_request (spinner, 32, 32);
  gtk_widget_add_css_class (spinner, "spinner");
  gtk_spinner_start (GTK_SPINNER (spinner));

  content = adw_status_page_new ();
  adw_status_page_set_icon_name (ADW_STATUS_PAGE (content), "video-display-symbolic");
  adw_status_page_set_title (ADW_STATUS_PAGE (content), "Waiting for a device");
  adw_status_page_set_description (ADW_STATUS_PAGE (content),
                                   "Open Quick Share / Cast on your device and select this computer.");
  viewport = gtk_stack_new ();
  gtk_stack_add_named (GTK_STACK (viewport), content, "idle");
  gtk_stack_add_named (GTK_STACK (viewport), GTK_WIDGET (self->video_picture), "video");

  self->stack = GTK_STACK (viewport);
  gtk_stack_set_transition_type (self->stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_widget_set_vexpand (viewport, TRUE);

  gtk_box_append (box, GTK_WIDGET (self->status_label));
  gtk_box_append (box, GTK_WIDGET (self->device_label));
  gtk_box_append (box, viewport);

  adw_toolbar_view_set_content (toolbar, GTK_WIDGET (box));
  gtk_window_set_child (GTK_WINDOW (self), GTK_WIDGET (toolbar));

  gtk_window_set_default_size (GTK_WINDOW (self), 720, 480);
  gtk_window_set_title (GTK_WINDOW (self), "Miracast Sink");
}

static void
msk_window_class_init (MskWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *props[LAST_PROP] = { NULL };

  object_class->set_property = msk_window_set_property;

  props[PROP_DISCONNECT] =
    g_param_spec_boolean ("disconnect",
                          "Disconnect",
                          "Request disconnection of the current stream",
                          FALSE,
                          G_PARAM_WRITABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_signal_new ("disconnect-requested",
                MSK_TYPE_WINDOW,
                G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE,
                0);
}

MskWindow *
msk_window_new (GtkApplication *app)
{
  return g_object_new (MSK_TYPE_WINDOW,
                       "application", app,
                       NULL);
}

void
msk_window_set_status (MskWindow *self, const char *status)
{
  gtk_label_set_text (self->status_label, status);
}

void
msk_window_set_device (MskWindow *self, const char *device)
{
  gtk_label_set_text (self->device_label, device);
}

void
msk_window_set_streaming (MskWindow *self, gboolean streaming)
{
  gtk_stack_set_visible_child_name (self->stack, streaming ? "video" : "idle");
  gtk_widget_set_sensitive (GTK_WIDGET (self->disconnect_button), streaming);
}

void
msk_window_show_video (MskWindow *self, GdkPaintable *paintable)
{
  gtk_picture_set_paintable (self->video_picture, paintable);
  msk_window_set_streaming (self, TRUE);
}

void
msk_window_clear_video (MskWindow *self)
{
  gtk_picture_set_paintable (self->video_picture, NULL);
  msk_window_set_streaming (self, FALSE);
}