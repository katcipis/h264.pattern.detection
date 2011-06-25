#!/bin/bash

file speed_bag_1080p.y4m

if test ! $? -eq 0; then
  wget http://media.xiph.org/video/derf/y4m/speed_bag_1080p.y4m
fi

gst-launch -v filesrc location=speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! filesink location=speed_bag_1080p.yuv
