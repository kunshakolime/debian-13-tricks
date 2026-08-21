#ifndef MSK_BACKEND_H
#define MSK_BACKEND_H

#include <glib-object.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define MSK_TYPE_BACKEND (msk_backend_get_type ())
G_DECLARE_INTERFACE (MskBackend, msk_backend, MSK, BACKEND, GObject)

struct _MskBackendInterface
{
  GTypeInterface parent_iface;

  void         (*start)           (MskBackend *self);
  void         (*stop)            (MskBackend *self);
  GdkPaintable *(*get_paintable)  (MskBackend *self);
  const char  *(*get_name)        (MskBackend *self);
};

void          msk_backend_start           (MskBackend *self);
void          msk_backend_stop            (MskBackend *self);
GdkPaintable *msk_backend_get_paintable   (MskBackend *self);
const char   *msk_backend_get_name        (MskBackend *self);

G_END_DECLS

#endif /* MSK_BACKEND_H */
