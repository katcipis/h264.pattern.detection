#!/bin/bash

if test ! -f pedestrian_area.yuv ; then

  if test ! -f pedestrian_area_720p25.y4m ; then
    wget http://media.xiph.org/video/derf/y4m/pedestrian_area_720p25.y4m
  fi

  echo "=== Generating YUV file ==="
  gst-launch filesrc location=pedestrian_area_720p25.y4m ! decodebin2 ! ffmpegcolorspace ! videoscale ! video/x-raw-yuv,format=\(fourcc\)I420,width=1280,height=720 ! filesink location=pedestrian_area.yuv
  rm pedestrian_area_720p25.y4m

fi

echo "=== Creating directory to put results ==="
mkdir -p results


echo "=== Starting original test ==="

cd ../../
./lencod.exe -f ./tests/pedestrian_area_720/encoder.cfg &> ./tests/pedestrian_area_720/results/encoder_original.res
cp ./tests/pedestrian_area_720/pedestrian_area.h264 ./tests/pedestrian_area_720/results/pedestrian_area_original.h264

echo "=== Starting 1/1 test ==="

./lencod.exe -f ./tests/pedestrian_area_720/encoder_1_1.cfg &> ./tests/pedestrian_area_720/results/encoder_1_1.res

cp ./tests/pedestrian_area_720/pedestrian_area.h264 ./tests/pedestrian_area_720/results/pedestrian_area_1_1.h264

./ldecod.exe -f ./tests/pedestrian_area_720/decoder.cfg

gst-launch-0.10 filesrc location=./tests/pedestrian_area_720/pedestrian_area_dec.yuv ! videoparse format=1 width=1280 height=720 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/pedestrian_area_720/results/pedestrian_area_1_1.ogg


echo "=== Starting 5/10 test ==="

./lencod.exe -f ./tests/pedestrian_area_720/encoder_5_10.cfg &> ./tests/pedestrian_area_720/results/encoder_5_10.res

cp ./tests/pedestrian_area_720/pedestrian_area.h264 ./tests/pedestrian_area_720/results/pedestrian_area_5_10.h264

./ldecod.exe -f ./tests/pedestrian_area_720/decoder.cfg 

gst-launch-0.10 filesrc location=./tests/pedestrian_area_720/pedestrian_area_dec.yuv ! videoparse format=1 width=1280 height=720 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/pedestrian_area_720/results/pedestrian_area_5_10.ogg


echo "=== Starting 10/30 test ==="

./lencod.exe -f ./tests/pedestrian_area_720/encoder_10_30.cfg &> ./tests/pedestrian_area_720/results/encoder_10_30.res

cp ./tests/pedestrian_area_720/pedestrian_area.h264 ./tests/pedestrian_area_720/results/pedestrian_area_10_30.h264

./ldecod.exe -f ./tests/pedestrian_area_720/decoder.cfg 

gst-launch-0.10 filesrc location=./tests/pedestrian_area_720/pedestrian_area_dec.yuv ! videoparse format=1 width=1280 height=720 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/pedestrian_area_720/results/pedestrian_area_10_30.ogg


echo "=== Starting 10/60 test ==="

./lencod.exe -f ./tests/pedestrian_area_720/encoder_10_60.cfg &> ./tests/pedestrian_area_720/results/encoder_10_60.res

cp ./tests/pedestrian_area_720/pedestrian_area.h264 ./tests/pedestrian_area_720/results/pedestrian_area_10_60.h264

./ldecod.exe -f ./tests/pedestrian_area_720/decoder.cfg 

gst-launch-0.10 filesrc location=./tests/pedestrian_area_720/pedestrian_area_dec.yuv ! videoparse format=1 width=1280 height=720 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/pedestrian_area_720/results/pedestrian_area_10_60.ogg

echo "=== We are done ==="
