#!/usr/bin/env bash
gst-launch-1.0 videotestsrc \
  ! video/x-raw,width=1280,height=720,framerate=30/1 \
  ! x264enc tune=zerolatency ! mpegtsmux ! rtpmp2tpay \
  ! udpsink host=127.0.0.1 port=7236
