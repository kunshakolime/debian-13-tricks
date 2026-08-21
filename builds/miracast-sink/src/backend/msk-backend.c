#include "msk-backend.h"

G_DEFINE_INTERFACE (MskBackend, msk_backend, G_TYPE_OBJECT)

static void
msk_backend_default_init (MskBackendInterface *iface)
{
}

void
msk_backend_start (MskBackend *self)
{
  MskBackendInterface *iface;

  g_return_if_fail (MSK_IS_BACKEND (self));

  iface = MSK_BACKEND_GET_IFACE (self);
  g_return_if_fail (iface->start != NULL);

  iface->start (self);
}

void
msk_backend_stop (MskBackend *self)
{
  MskBackendInterface *iface;

  g_return_if_fail (MSK_IS_BACKEND (self));

  iface = MSK_BACKEND_GET_IFACE (self);
  g_return_if_fail (iface->stop != NULL);

  iface->stop (self);
}

GdkPaintable *
msk_backend_get_paintable (MskBackend *self)
{
  MskBackendInterface *iface;

  g_return_val_if_fail (MSK_IS_BACKEND (self), NULL);

  iface = MSK_BACKEND_GET_IFACE (self);
  g_return_val_if_fail (iface->get_paintable != NULL, NULL);

  return iface->get_paintable (self);
}

const char *
msk_backend_get_name (MskBackend *self)
{
  MskBackendInterface *iface;

  g_return_val_if_fail (MSK_IS_BACKEND (self), NULL);

  iface = MSK_BACKEND_GET_IFACE (self);
  if (iface->get_name)
    return iface->get_name (self);

  return G_OBJECT_TYPE_NAME (self);
}
