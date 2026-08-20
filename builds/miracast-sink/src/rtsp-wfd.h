#ifndef MSK_RTSP_WFD_H
#define MSK_RTSP_WFD_H

#include <glib-object.h>
#include "common.h"

G_BEGIN_DECLS

#define MSK_TYPE_RTSP_WFD (msk_rtsp_wfd_get_type ())
G_DECLARE_FINAL_TYPE (MskRtspWfd, msk_rtsp_wfd, MSK, RTSP_WFD, GObject)

#define MSK_RTSP_WFD_SIGNAL_STREAM_READY "stream-ready"

MskRtspWfd *msk_rtsp_wfd_new (guint16 port, GError **error);
void        msk_rtsp_wfd_start (MskRtspWfd *self);
void        msk_rtsp_wfd_stop  (MskRtspWfd *self);
void        msk_rtsp_wfd_connect_peer (MskRtspWfd *self, const char *peer_address);

G_END_DECLS

#endif /* MSK_RTSP_WFD_H */