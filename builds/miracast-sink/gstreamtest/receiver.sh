#!/usr/bin/env bash
gst-launch-1.0 udpsrc port=7236 caps="application/x-rtp, media=video" \
  ! rtpjitterbuffer ! rtpmp2tdepay ! tsdemux ! queue \
  ! h264parse ! avdec_h264 ! videoconvert ! autovideosink
