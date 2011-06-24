#!/bin/bash

file news_cif.y4m

if test ! $? -eq 0; then
  echo ""
  echo "Downloading video News CIF"
  echo ""

  wget http://media.xiph.org/video/derf/y4m/news_cif.y4m
fi

file crowd_run_1080p50.y4m

if test ! $? -eq 0; then
  echo "Downloading video Crowd 1080p"
  wget http://media.xiph.org/video/derf/y4m/crowd_run_1080p50.y4m
fi

file speed_bag_1080p.y4m

if test ! $? -eq 0; then
  echo "Downloading video Speed bag 1080"
  wget http://media.xiph.org/video/derf/y4m/speed_bag_1080p.y4m
fi

#Scaling tests

echo ""
echo "Generating 1080p YUV videos"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! filesink location=crowd_run_1080p.yuv
gst-launch -v filesrc location=speed_bag_1080p.y4m ! decodebin2 ! filesink location=speed_bag_1080p.yuv

echo ""
echo "Generating CIF videos"
echo ""

gst-launch -v filesrc location=news_cif.y4m ! decodebin2 ! filesink location=news_cif.yuv

echo ""
echo "Generating 720p YUV videos"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=1280,height=720 ! filesink location=crowd_run_720p.yuv
gst-launch -v filesrc location=speed_bag_1080p.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=1280,height=720 ! filesink location=speed_bag_720p.yuv

echo ""
echo "Generating 640 x 360 YUV 4:2:0 video"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=640,height=360 ! filesink location=crowd_run_640_360.yuv
gst-launch -v filesrc location=speed_bag_1080p.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=640,height=360 ! filesink location=speed_bag_640_360.yuv

echo ""
echo "Generating 320 x 180 YUV 4:2:0 video"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=320,height=180 ! filesink location=crowd_run_320_180.yuv
gst-launch -v filesrc location=speed_bag_1080p.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=320,height=180 ! filesink location=speed_bag_320_180p.yuv


# bitrate tests
echo ""
echo "Generating H.264 encoded bitrate=2048"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! x264enc bitrate=2048 ! decodebin2 ! filesink location=crowd_run_1080p_bitrate_2048.yuv
gst-launch -v filesrc location=speed_bag_1080p.y4m ! decodebin2 ! x264enc bitrate=2048 ! decodebin2 ! filesink location=speed_bag_1080p_bitrate_2048.yuv

echo ""
echo "Generating H.264 encoded bitrate=1024"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! x264enc bitrate=1024 ! decodebin2 ! filesink location=crowd_run_1080p_bitrate_1024.yuv
gst-launch -v filesrc location=speed_bag_1080p.y4m ! decodebin2 ! x264enc bitrate=1024 ! decodebin2 ! filesink location=speed_bag_1080p_bitrate_1024.yuv

echo ""
echo "Generating H.264 encoded bitrate=512"
echo ""

gst-launch -v filesrc location=news_cif.y4m ! decodebin2 ! x264enc bitrate=512 ! decodebin2 ! filesink location=news_cif_bitrate_512.yuv
gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! x264enc bitrate=512 ! decodebin2 ! filesink location=crowd_run_1080p_bitrate_512.yuv
gst-launch -v filesrc location=speed_bag_1080p.y4m ! decodebin2 ! x264enc bitrate=512 ! decodebin2 ! filesink location=speed_bag_1080p_bitrate_512.yuv

echo ""
echo "Generating H.264 encoded bitrate=256"
echo ""

gst-launch -v filesrc location=news_cif.y4m ! decodebin2 ! x264enc bitrate=256 ! decodebin2 ! filesink location=news_cif_bitrate_256.yuv
gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! x264enc bitrate=256 ! decodebin2 ! filesink location=crowd_run_1080p_bitrate_256.yuv
gst-launch -v filesrc location=speed_bag_1080p.y4m ! decodebin2 ! x264enc bitrate=256 ! decodebin2 ! filesink location=speed_bag_1080p_bitrate_256.yuv

echo ""
echo "Generating H.264 encoded bitrate=128"
echo ""

gst-launch -v filesrc location=news_cif.y4m ! decodebin2 ! x264enc bitrate=128 ! decodebin2 ! filesink location=news_cif_bitrate_128.yuv
gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! x264enc bitrate=128 ! decodebin2 ! filesink location=crowd_run_1080p_bitrate_128.yuv
gst-launch -v filesrc location=speed_bag_1080p.y4m ! decodebin2 ! x264enc bitrate=128 ! decodebin2 ! filesink location=speed_bag_1080p_bitrate_128.yuv
