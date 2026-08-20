#include "stream.h"

#include <gst/gst.h>
#include <gst/video/video.h>

struct _MskStream
{
  GObject parent_instance;

  guint16 port;
  gboolean running;

  GstElement *pipeline;
  GstElement *udpsrc;
  GstElement *tsdemux;
  GstElement *queue;
  GstElement *h264parse;
  GstElement *decoder;
  GstElement *videoconvert;
  GstElement *sink;
  GdkPaintable *paintable;
};

G_DEFINE_TYPE (MskStream, msk_stream, G_TYPE_OBJECT)

static void
on_bus_error (GstBus *bus, GstMessage *msg, gpointer user_data)
{
  GError *err = NULL;
  gchar *dbg = NULL;

  gst_message_parse_error (msg, &err, &dbg);
  g_warning ("GStreamer error: %s (%s)",
             err ? err->message : "unknown", dbg ? dbg : "");
  g_clear_error (&err);
  g_free (dbg);
}

static void
on_bus_eos (GstBus *bus, GstMessage *msg, gpointer user_data)
{
  g_debug ("GStreamer EOS");
}

static void
on_tsdemux_pad_added (GstElement *src, GstPad *new_pad, gpointer user_data)
{
  MskStream *self = MSK_STREAM (user_data);
  GstPad *sink_pad = NULL;
  GstCaps *caps = NULL;
  GstStructure *s = NULL;
  const gchar *media_type = NULL;

  caps = gst_pad_get_current_caps (new_pad);
  if (!caps)
    caps = gst_pad_query_caps (new_pad, NULL);
  s = gst_caps_get_structure (caps, 0);
  media_type = gst_structure_get_name (s);

  if (g_str_has_prefix (media_type, "video/"))
    {
      sink_pad = gst_element_get_static_pad (self->queue, "sink");
      if (!gst_pad_is_linked (sink_pad))
        {
          GstPadLinkReturn ret = gst_pad_link (new_pad, sink_pad);
          if (GST_PAD_LINK_FAILED (ret))
            g_warning ("Failed to link tsdemux video pad: %d", ret);
          else
            g_debug ("Linked tsdemux video pad to queue");
        }
      gst_object_unref (sink_pad);
    }

  gst_caps_unref (caps);
}

void
msk_stream_start (MskStream *self)
{
  if (self->running)
    return;

  gst_element_set_state (self->pipeline, GST_STATE_PLAYING);
  self->running = TRUE;
}

void
msk_stream_stop (MskStream *self)
{
  if (!self->running)
    return;

  self->running = FALSE;
  gst_element_set_state (self->pipeline, GST_STATE_NULL);

  if (self->udpsrc)
    g_object_set (self->udpsrc, "port", self->port, NULL);

  /* release dynamic pads on tsdemux so a restart works */
  if (self->tsdemux)
    gst_element_release_request_pads (self->tsdemux);
}

GdkPaintable *
msk_stream_get_paintable (MskStream *self)
{
  return self->paintable;
}

static void
msk_stream_finalize (GObject *object)
{
  MskStream *self = MSK_STREAM (object);

  if (self->pipeline)
    {
      gst_element_set_state (self->pipeline, GST_STATE_NULL);
      gst_object_unref (self->pipeline);
    }
  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (msk_stream_parent_class)->finalize (object);
}

static void
msk_stream_class_init (MskStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = msk_stream_finalize;
}

static void
msk_stream_init (MskStream *self)
{
}

MskStream *
msk_stream_new (guint16 port)
{
  MskStream *self;
  GstBus *bus;

  self = g_object_new (MSK_TYPE_STREAM, NULL);
  self->port = port ? port : 7236;

  self->pipeline      = gst_pipeline_new ("miracast-sink");
  self->udpsrc        = gst_element_factory_make ("udpsrc", "udpsrc");
  GstElement *jitbuf  = gst_element_factory_make ("rtpjitterbuffer", NULL);
  GstElement *depay   = gst_element_factory_make ("rtpmp2tdepay", NULL);
  self->tsdemux       = gst_element_factory_make ("tsdemux", NULL);
  self->queue         = gst_element_factory_make ("queue", NULL);
  self->h264parse     = gst_element_factory_make ("h264parse", NULL);
  self->decoder       = gst_element_factory_make ("avdec_h264", NULL);
  self->videoconvert  = gst_element_factory_make ("videoconvert", NULL);
  self->sink          = gst_element_factory_make ("gtk4paintablesink", "gtk4sink");

  g_assert (self->udpsrc && jitbuf && depay && self->tsdemux &&
            self->queue && self->h264parse && self->decoder &&
            self->videoconvert && self->sink);

  g_object_set (self->udpsrc,
                "port", self->port,
                "caps",
                gst_caps_from_string ("application/x-rtp, media=video"),
                NULL);

  gst_bin_add_many (GST_BIN (self->pipeline),
                    self->udpsrc, jitbuf, depay, self->tsdemux,
                    self->queue, self->h264parse, self->decoder,
                    self->videoconvert, self->sink, NULL);

  /* link statically up to tsdemux (its src pads are dynamic) */
  gst_element_link_many (self->udpsrc, jitbuf, depay, self->tsdemux, NULL);

  /* link downstream of tsdemux statically (queue -> h264parse -> decode -> convert -> sink) */
  gst_element_link_many (self->queue, self->h264parse, self->decoder,
                         self->videoconvert, self->sink, NULL);

  /* connect dynamic pads from tsdemux */
  g_signal_connect (self->tsdemux, "pad-added",
                    G_CALLBACK (on_tsdemux_pad_added), self);

  bus = gst_element_get_bus (self->pipeline);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::error", G_CALLBACK (on_bus_error), self);
  g_signal_connect (bus, "message::eos", G_CALLBACK (on_bus_eos), self);
  gst_object_unref (bus);

  g_object_get (self->sink, "paintable", &self->paintable, NULL);

  return self;
}