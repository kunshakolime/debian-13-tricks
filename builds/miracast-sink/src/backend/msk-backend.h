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

/* ------------------------------------------------------------------ */
/* Shared backend signal helpers — include in each backend .c file     */
/* after the signals[] enum and G_DEFINE_TYPE_WITH_CODE.               */
/* ------------------------------------------------------------------ */

#define MSK_BACKEND_SIGNAL_DEFS(signals, TYPE) \
static void \
msk_emit_connected (gpointer instance, const char *name) \
{ \
  g_signal_emit (instance, signals[SIGNAL_CONNECTED], 0, name); \
} \
\
static void \
msk_emit_disconnected (gpointer instance) \
{ \
  g_signal_emit (instance, signals[SIGNAL_DISCONNECTED], 0); \
} \
\
static void \
msk_emit_status (gpointer instance, const char *status) \
{ \
  g_signal_emit (instance, signals[SIGNAL_STATUS_CHANGED], 0, status); \
} \
\
static void \
msk_register_signals (void) \
{ \
  signals[SIGNAL_CONNECTED] = \
    g_signal_new ("connected", TYPE, G_SIGNAL_RUN_FIRST, \
                  0, NULL, NULL, NULL, \
                  G_TYPE_NONE, 1, G_TYPE_STRING); \
  signals[SIGNAL_DISCONNECTED] = \
    g_signal_new ("disconnected", TYPE, G_SIGNAL_RUN_FIRST, \
                  0, NULL, NULL, NULL, \
                  G_TYPE_NONE, 0); \
  signals[SIGNAL_STATUS_CHANGED] = \
    g_signal_new ("status-changed", TYPE, G_SIGNAL_RUN_FIRST, \
                  0, NULL, NULL, NULL, \
                  G_TYPE_NONE, 1, G_TYPE_STRING); \
}
