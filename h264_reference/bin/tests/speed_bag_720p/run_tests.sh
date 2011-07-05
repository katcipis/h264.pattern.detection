#!/bin/bash

if test ! -f speed_bag.yuv ; then

  if test ! -f speed_bag_1080p.y4m ; then
    wget wget http://media.xiph.org/video/derf/y4m/speed_bag_1080p.y4m
  fi

  echo "=== Generating YUV file ==="
  gst-launch filesrc location=speed_bag_1080p.y4m ! decodebin2 ! ffmpegcolorspace ! videoscale ! video/x-raw-yuv,format=\(fourcc\)I420,width=1280,height=720 ! filesink location=speed_bag.yuv
  rm speed_bag_1080p.y4m

fi

echo "=== Creating directory to put results ==="
mkdir -p results


echo "=== Starting original test ==="

cd ../../

./lencod.exe -f ./tests/speed_bag/encoder.cfg &> ./tests/speed_bag/results/encoder_original.res

cp ./tests/speed_bag/speed_bag.h264 ./tests/speed_bag/results/speed_bag_original.h264


echo "=== Starting 1/1 test ==="

./lencod.exe -f ./tests/speed_bag/encoder_1_1.cfg &> ./tests/speed_bag/results/encoder_1_1.res

cp ./tests/speed_bag/speed_bag.h264 ./tests/speed_bag/results/speed_bag_1_1.h264

./ldecod.exe -f ./tests/speed_bag/decoder.cfg

gst-launch-0.10 filesrc location=./tests/speed_bag/speed_bag_dec.yuv ! videoparse format=1 width=1280 height=720 framerate=30000/1001 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/speed_bag/results/speed_bag_1_1.ogg


echo "=== Starting 5/10 test ==="

./lencod.exe -f ./tests/speed_bag/encoder_5_10.cfg &> ./tests/speed_bag/results/encoder_5_10.res

cp ./tests/speed_bag/speed_bag.h264 ./tests/speed_bag/results/speed_bag_5_10.h264

./ldecod.exe -f ./tests/speed_bag/decoder.cfg 

gst-launch-0.10 filesrc location=./tests/speed_bag/speed_bag_dec.yuv ! videoparse format=1 width=1280 height=720 framerate=30000/1001 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/speed_bag/results/speed_bag_5_10.ogg


echo "=== Starting 10/30 test ==="

./lencod.exe -f ./tests/speed_bag/encoder_10_30.cfg &> ./tests/speed_bag/results/encoder_10_30.res

cp ./tests/speed_bag/speed_bag.h264 ./tests/speed_bag/results/speed_bag_10_30.h264

./ldecod.exe -f ./tests/speed_bag/decoder.cfg 

gst-launch-0.10 filesrc location=./tests/speed_bag/speed_bag_dec.yuv ! videoparse format=1 width=1280 height=720 framerate=30000/1001 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/speed_bag/results/speed_bag_10_30.ogg


echo "=== Starting 10/60 test ==="

./lencod.exe -f ./tests/speed_bag/encoder_10_60.cfg &> ./tests/speed_bag/results/encoder_10_60.res

cp ./tests/speed_bag/speed_bag.h264 ./tests/speed_bag/results/speed_bag_10_60.h264

./ldecod.exe -f ./tests/speed_bag/decoder.cfg 

gst-launch-0.10 filesrc location=./tests/speed_bag/speed_bag_dec.yuv ! videoparse format=1 width=1280 height=720 framerate=30000/1001 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/speed_bag/results/speed_bag_10_60.ogg

echo "=== We are done ==="
