#!/bin/bash

file crowd_run_1080p50.y4m

if test ! $? -eq 0; then
  echo "Downloading video for performance tests"
  #wget http://media.xiph.org/video/derf/y4m/crowd_run_1080p50.y4m
fi


echo ""
echo "Generating 1080p YUV 4:2:0 video"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! filesink location=crowd_run_1080p.yuv

echo ""
echo "Generating 720p YUV 4:2:0 video"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=1280,height=720 ! filesink location=crowd_run_720p.yuv

echo ""
echo "Generating 640 x 360 YUV 4:2:0 video"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=640,height=360 ! filesink location=crowd_run_640_360.yuv

echo ""
echo "Generating 320 x 180 YUV 4:2:0 video"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=320,height=180 ! filesink location=crowd_run_320_180.yuv

# QUALITY TESTS

file news_cif.y4m

if test ! $? -eq 0; then
  echo ""
  echo "Downloading video for different video quality tests"
  echo ""

  wget http://media.xiph.org/video/derf/y4m/news_cif.y4m
fi

echo ""
echo "Generating QCIF YUV 4:2:0 video, original bitrate"
echo ""

gst-launch -v filesrc location=news_cif.y4m ! decodebin2 ! filesink location=news_cif.yuv

echo ""
echo "Generating QCIF YUV 4:2:0 video, H.264 encoded bitrate=512"
echo ""

gst-launch -v filesrc location=news_cif.y4m ! decodebin2 ! x264enc bitrate=512 ! decodebin2 ! filesink location=news_cif_512.yuv

echo ""
echo "Generating QCIF YUV 4:2:0 video, H.264 encoded bitrate=256"
echo ""

gst-launch -v filesrc location=news_cif.y4m ! decodebin2 ! x264enc bitrate=256 ! decodebin2 ! filesink location=news_cif_256.yuv

echo ""
echo "Generating QCIF YUV 4:2:0 video, H.264 encoded bitrate=128"
echo ""

gst-launch -v filesrc location=news_cif.y4m ! decodebin2 ! x264enc bitrate=128 ! decodebin2 ! filesink location=news_cif_128.yuv
