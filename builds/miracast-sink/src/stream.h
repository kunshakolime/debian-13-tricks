#ifndef MSK_STREAM_H
#define MSK_STREAM_H

#include <glib-object.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define MSK_TYPE_STREAM (msk_stream_get_type ())
G_DECLARE_FINAL_TYPE (MskStream, msk_stream, MSK, STREAM, GObject)

MskStream *msk_stream_new (guint16 port);

void         msk_stream_start (MskStream *self);
void         msk_stream_stop  (MskStream *self);
GdkPaintable *msk_stream_get_paintable (MskStream *self);

G_END_DECLS

#endif /* MSK_STREAM_H */