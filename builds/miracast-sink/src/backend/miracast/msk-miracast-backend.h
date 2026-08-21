#ifndef MSK_MIRACAST_BACKEND_H
#define MSK_MIRACAST_BACKEND_H

#include <glib-object.h>
#include "../msk-backend.h"

G_BEGIN_DECLS

#define MSK_TYPE_MIRACAST_BACKEND (msk_miracast_backend_get_type ())
G_DECLARE_FINAL_TYPE (MskMiracastBackend, msk_miracast_backend, MSK, MIRACAST_BACKEND, GObject)

MskBackend *msk_miracast_backend_new (void);

G_END_DECLS

#endif /* MSK_MIRACAST_BACKEND_H */
