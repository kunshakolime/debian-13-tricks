#ifndef MSK_WINDOW_H
#define MSK_WINDOW_H

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define MSK_TYPE_WINDOW (msk_window_get_type ())
G_DECLARE_FINAL_TYPE (MskWindow, msk_window, MSK, WINDOW, AdwApplicationWindow)

MskWindow *msk_window_new (GtkApplication *app);

void msk_window_set_status          (MskWindow *self, const char *status);
void msk_window_set_device          (MskWindow *self, const char *device);
void msk_window_set_backend         (MskWindow *self, const char *backend);
void msk_window_set_backend_status  (MskWindow *self, const char *name, const char *status);
void msk_window_set_streaming       (MskWindow *self, gboolean streaming);
void msk_window_show_video          (MskWindow *self, GdkPaintable *paintable);
void msk_window_clear_video         (MskWindow *self);

G_END_DECLS

#endif /* MSK_WINDOW_H */
