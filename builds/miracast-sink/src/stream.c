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
  GstElement *rtpjitterbuffer, *depay, *tsdemux, *queue;
  GstElement *h264parse, *decoder, *videoconvert, *sink;
  GstBus *bus;

  self = g_object_new (MSK_TYPE_STREAM, NULL);
  self->port = port ? port : 7236;

  self->pipeline = gst_pipeline_new ("miracast-sink");
  self->udpsrc = gst_element_factory_make ("udpsrc", "udpsrc");
  rtpjitterbuffer = gst_element_factory_make ("rtpjitterbuffer", NULL);
  depay = gst_element_factory_make ("rtpmp2tdepay", NULL);
  tsdemux = gst_element_factory_make ("tsdemux", NULL);
  queue = gst_element_factory_make ("queue", NULL);
  h264parse = gst_element_factory_make ("h264parse", NULL);
  decoder = gst_element_factory_make ("avdec_h264", NULL);
  videoconvert = gst_element_factory_make ("videoconvert", NULL);
  sink = gst_element_factory_make ("gtk4paintablesink", "gtk4sink");

  g_assert (self->udpsrc && rtpjitterbuffer && depay && tsdemux &&
            queue && h264parse && decoder && videoconvert && sink);

  self->sink = sink;

  g_object_set (self->udpsrc,
                "port", self->port,
                "caps",
                gst_caps_from_string ("application/x-rtp, media=video"),
                NULL);

  gst_bin_add_many (GST_BIN (self->pipeline),
                    self->udpsrc, rtpjitterbuffer, depay, tsdemux, queue,
                    h264parse, decoder, videoconvert, sink, NULL);

  gst_element_link_many (self->udpsrc, rtpjitterbuffer, depay, tsdemux,
                         queue, h264parse, decoder, videoconvert, sink,
                         NULL);

  bus = gst_element_get_bus (self->pipeline);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::error", G_CALLBACK (on_bus_error), self);
  g_signal_connect (bus, "message::eos", G_CALLBACK (on_bus_eos), self);
  gst_object_unref (bus);

  g_object_get (sink, "paintable", &self->paintable, NULL);

  return self;
}