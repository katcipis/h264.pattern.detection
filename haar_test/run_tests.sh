#!/bin/bash

mkdir -p videos
cd videos

if test ! -f news_cif.y4m ; then
  echo "Downloading video News CIF"
  wget http://media.xiph.org/video/derf/y4m/news_cif.y4m
fi

if test ! -f crowd_run_1080p50.y4m ; then
  echo "Downloading video Crowd 1080p"
  wget http://media.xiph.org/video/derf/y4m/crowd_run_1080p50.y4m
fi

if test ! -f speed_bag_1080p.y4m ; then
  echo "Downloading video Speed bag 1080"
  wget http://media.xiph.org/video/derf/y4m/speed_bag_1080p.y4m
fi

if test ! -f blue_sky_1080p25.y4m ; then
  echo "Downloading video Blue Sky 1080"
  wget http://media.xiph.org/video/derf/y4m/blue_sky_1080p25.y4m
fi

cd ..
mkdir -p results

#Scaling tests

width=0
height=0

run_scale_tests()
{
  eval "gst-launch filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! videoscale ! video/x-raw-yuv,width=$width,height=$height ! filesink location=./videos/crowd_run_$width_$height.yuv"

  eval "gst-launch filesrc location=./videos/blue_sky_1080p25.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! videoscale ! video/x-raw-yuv,width=$width,height=$height ! filesink location=./videos/blue_sky_$width_$height.yuv"

  eval "gst-launch filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! videoscale ! video/x-raw-yuv,width=$width,height=$height ! filesink location=./videos/speed_bag_$width_$height.yuv"


  eval "./haar-test haarcascade_frontalface_alt.xml ./videos/crowd_run_$width_$height.yuv $width $height &> results/crowd_run_$width_$height.result"
  rm "./videos/crowd_run_$width_$height.yuv"

  eval "./haar-test haarcascade_frontalface_alt.xml ./videos/blue_sky_$width_$height.yuv $width $height &> results/blue_sky_$width_$height.result"
  rm "./videos/blue_sky_$width_$height.yuv"

  eval "./haar-test haarcascade_frontalface_alt.xml ./videos/speed_bag_$width_$height.yuv $width $height &> results/speed_bag_$width_$height.yuv.result"
  rm "./videos/speed_bag_$width_$height.yuv"
}

echo ""
echo "Starting scaling tests"
echo "Running 1080p tests"
echo ""

width=1920
height=1080

run_scale_tests


echo ""
echo "Running 720p Tests"
echo ""

width=1280
height=720

run_scale_tests


echo ""
echo "Running 640 x 360 Tests"
echo ""

width=640
height=360

run_scale_tests

echo ""
echo "Running 320 x 180 Tests"
echo ""

width=320
height=180

run_scale_tests


echo ""
echo "Starting bitrate tests"
echo ""

# bitrate tests

bitrate=0

run_bitrate_tests()
{

  eval "gst-launch filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! x264enc bitrate=$bitrate ! decodebin2 ! filesink location=./videos/crowd_run_1080p_bitrate_$bitrate.yuv"

  eval "gst-launch  filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! x264enc bitrate=$bitrate ! decodebin2 ! filesink location=./videos/speed_bag_1080p_bitrate_$bitrate.yuv"

  eval "gst-launch  filesrc location=./videos/blue_sky_1080p25.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! x264enc bitrate=$bitrate ! decodebin2 ! filesink location=./videos/blue_sky_1080p_bitrate_$bitrate.yuv"


  eval "./haar-test haarcascade_frontalface_alt.xml ./videos/crowd_run_1080p_bitrate_$bitrate.yuv 1920 1080 &> results/crowd_run_1080p_bitrate_$bitrate.result"
  rm "./videos/crowd_run_1080p_bitrate_$bitrate.yuv"

  eval "./haar-test haarcascade_frontalface_alt.xml ./videos/blue_sky_1080p_bitrate_$bitrate.yuv 1920 1080 &> results/blue_sky_1080p_bitrate_$bitrate.result"
  rm "./videos/blue_sky_1080p_bitrate_$bitrate.yuv"

  eval "./haar-test haarcascade_frontalface_alt.xml ./videos/speed_bag_1080p_bitrate_$bitrate.yuv 1920 1080 &> results/speed_bag_1080p_bitrate_$bitrate.yuv.result"
  rm "./videos/speed_bag_1080p_bitrate_$bitrate.yuv"
}

echo ""
echo "Running H.264 encoded bitrate=2048"
echo ""

bitrate=2048
run_bitrate_tests

echo ""
echo "Running H.264 encoded bitrate=1024"
echo ""

bitrate=1024
run_bitrate_tests

echo ""
echo "Running H.264 encoded bitrate=512"
echo ""

bitrate=512
run_bitrate_tests

echo ""
echo "Running H.264 encoded bitrate=256"
echo ""

bitrate=256
run_bitrate_tests

echo ""
echo "Running H.264 encoded bitrate=128"
echo ""

bitrate=128
run_bitrate_tests

echo "=== Cleaning up YUV videos ==="
rm *.yuv
rm ./videos/*.yuv
