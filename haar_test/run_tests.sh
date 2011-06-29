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

echo ""
echo "Starting scaling tests"
echo "Running 1080p tests"
echo ""

gst-launch  filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! filesink location=./videos/crowd_run_1080p.yuv

gst-launch  filesrc location=./videos/blue_sky_1080p25.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! filesink location=./videos/blue_sky_1080p.yuv

gst-launch  filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! filesink location=./videos/speed_bag_1080p.yuv


./haar-test haarcascade_frontalface_alt.xml ./videos/crowd_run_1080p.yuv 1920 1080 &> results/crowd_run_1080p.result
rm ./videos/crowd_run_1080p.yuv

./haar-test haarcascade_frontalface_alt.xml ./videos/blue_sky_1080p.yuv 1920 1080 &> results/blue_sky_1080p.result
rm ./videos/blue_sky_1080p.yuv

./haar-test haarcascade_frontalface_alt.xml ./videos/speed_bag_1080p.yuv 1920 1080 &> results/speed_bag_1080p.yuv.result
rm ./videos/speed_bag_1080p.yuv


echo ""
echo "Running CIF tests"
echo ""

gst-launch -v filesrc location=./videos/news_cif.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! filesink location=./videos/news_cif.yuv

./haar-test haarcascade_frontalface_alt.xml ./videos/news_cif.yuv 352 288 &> results/news_cif.yuv.result
rm ./videos/news_cif.yuv


echo ""
echo "Running 720p Tests"
echo ""

gst-launch filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=1280,height=720 ! filesink location=./videos/crowd_run_720p.yuv

gst-launch filesrc location=./videos/blue_sky_1080p25.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! videoscale ! video/x-raw-yuv,width=1280,height=720 ! filesink location=./videos/blue_sky_720p.yuv

gst-launch filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! videoscale ! video/x-raw-yuv,width=1280,height=720 ! filesink location=./videos/speed_bag_720p.yuv


./haar-test haarcascade_frontalface_alt.xml ./videos/crowd_run_720p.yuv 1280 720 &> results/crowd_run_720p.result
rm ./videos/crowd_run_720p.yuv

./haar-test haarcascade_frontalface_alt.xml ./videos/blue_sky_720p.yuv 1280 720 &> results/blue_sky_720p.result
rm ./videos/blue_sky_720p.yuv

./haar-test haarcascade_frontalface_alt.xml ./videos/speed_bag_720p.yuv 1280 720 &> results/speed_bag_720p.yuv.result
rm ./videos/speed_bag_720p.yuv


echo ""
echo "Running 640 x 360 Tests"
echo ""

gst-launch filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! videoscale ! video/x-raw-yuv,width=640,height=360 ! filesink location=./videos/crowd_run_640_360.yuv

gst-launch filesrc location=./videos/blue_sky_1080p25.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! videoscale ! video/x-raw-yuv,width=640,height=360 ! filesink location=./videos/blue_sky_640_360.yuv

gst-launch filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! videoscale ! video/x-raw-yuv,width=640,height=360 ! filesink location=./videos/speed_bag_640_360.yuv


./haar-test haarcascade_frontalface_alt.xml ./videos/crowd_run_640_360.yuv 1280 720 &> results/crowd_run_640_360.result
rm ./videos/crowd_run_640_360.yuv

./haar-test haarcascade_frontalface_alt.xml ./videos/blue_sky_640_360.yuv 1280 720 &> results/blue_sky_640_360.result
rm ./videos/blue_sky_640_360.yuv

./haar-test haarcascade_frontalface_alt.xml ./videos/speed_bag_640_360.yuv 1280 720 &> results/speed_bag_640_360.yuv.result
rm ./videos/speed_bag_640_360.yuv


echo ""
echo "Generating 320 x 180 YUV 4:2:0 video"
echo ""

#gst-launch  filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=320,height=180 ! filesink location=./videos/crowd_run_320_180.yuv

#gst-launch -v filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! videoscale ! video/x-raw-yuv,width=320,height=180 ! filesink location=./videos/speed_bag_320_180p.yuv


echo ""
echo "Starting bitrate tests"
echo ""

# bitrate tests
echo ""
echo "Running H.264 encoded bitrate=2048"
echo ""

#gst-launch -v filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! x264enc bitrate=2048 ! decodebin2 ! filesink location=./videos/crowd_run_1080p_bitrate_2048.yuv
#gst-launch -v filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! x264enc bitrate=2048 ! decodebin2 ! filesink location=./videos/speed_bag_1080p_bitrate_2048.yuv

echo ""
echo "Running H.264 encoded bitrate=1024"
echo ""

#gst-launch -v filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! x264enc bitrate=1024 ! decodebin2 ! filesink location=./videos/crowd_run_1080p_bitrate_1024.yuv
#gst-launch -v filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! x264enc bitrate=1024 ! decodebin2 ! filesink location=./videos/speed_bag_1080p_bitrate_1024.yuv

echo ""
echo "Running H.264 encoded bitrate=512"
echo ""

#gst-launch -v filesrc location=./videos/news_cif.y4m ! decodebin2 ! x264enc bitrate=512 ! decodebin2 ! filesink location=./videos/news_cif_bitrate_512.yuv
#gst-launch -v filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! x264enc bitrate=512 ! decodebin2 ! filesink location=./videos/crowd_run_1080p_bitrate_512.yuv
#gst-launch -v filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! x264enc bitrate=512 ! decodebin2 ! filesink location=./videos/speed_bag_1080p_bitrate_512.yuv

echo ""
echo "Running H.264 encoded bitrate=256"
echo ""

#gst-launch -v filesrc location=./videos/news_cif.y4m ! decodebin2 ! x264enc bitrate=256 ! decodebin2 ! filesink location=./videos/news_cif_bitrate_256.yuv
#gst-launch -v filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! x264enc bitrate=256 ! decodebin2 ! filesink location=./videos/crowd_run_1080p_bitrate_256.yuv
#gst-launch -v filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! x264enc bitrate=256 ! decodebin2 ! filesink location=./videos/speed_bag_1080p_bitrate_256.yuv

echo ""
echo "Running H.264 encoded bitrate=128"
echo ""

#gst-launch -v filesrc location=./videos/news_cif.y4m ! decodebin2 ! x264enc bitrate=128 ! decodebin2 ! filesink location=./videos/news_cif_bitrate_128.yuv
#gst-launch -v filesrc location=./videos/crowd_run_1080p50.y4m ! decodebin2 ! x264enc bitrate=128 ! decodebin2 ! filesink location=./videos/crowd_run_1080p_bitrate_128.yuv
#gst-launch -v filesrc location=./videos/speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! video/x-raw-yuv,format=\(fourcc\)I420 ! x264enc bitrate=128 ! decodebin2 ! filesink location=./videos/speed_bag_1080p_bitrate_128.yuv

echo "=== Cleaning up YUV videos ==="
rm *.yuv
rm ./videos/*.yuv
