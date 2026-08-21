#ifndef MSK_CHROMECAST_BACKEND_H
#define MSK_CHROMECAST_BACKEND_H

#include <glib-object.h>
#include "../msk-backend.h"

G_BEGIN_DECLS

#define MSK_TYPE_CHROMECAST_BACKEND (msk_chromecast_backend_get_type ())
G_DECLARE_FINAL_TYPE (MskChromecastBackend, msk_chromecast_backend, MSK, CHROMECAST_BACKEND, GObject)

MskBackend *msk_chromecast_backend_new (void);

G_END_DECLS

#endif /* MSK_CHROMECAST_BACKEND_H */
