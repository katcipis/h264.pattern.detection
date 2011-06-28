#!/bin/bash

if test ! -f pedestrian_area.yuv ; then
  if test ! -f pedestrian_area_1080p25.y4m ; then
    wget http://media.xiph.org/video/derf/y4m/pedestrian_area_1080p25.y4m
  fi
fi

echo "=== Generating YUV file ==="

gst-launch filesrc location=pedestrian_area_1080p25.y4m ! decodebin2 ! filesink location=pedestrian_area.yuv
rm pedestrian_area_1080p25.y4m


echo "=== Creating directory to put results ==="
mkdir -p results


echo "=== Starting original test ==="

cd ../../
./lencod.exe -f ./tests/pedestrian_area/encoder.cfg &> ./tests/pedestrian_area/results/encoder_original.res


echo "=== Starting 1/1 test ==="

./lencod.exe -f ./tests/pedestrian_area/encoder_1_1.cfg &> ./tests/pedestrian_area/results/encoder_1_1.res
./ldecod.exe -f ./tests/pedestrian_area/decoder.cfg
gst-launch-0.10 filesrc location=./tests/pedestrian_area/pedestrian_area_dec.yuv ! videoparse format=1 width=1920 height=1080 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/pedestrian_area/results/pedestrian_area_1_1.ogg


echo "=== Starting 5/10 test ==="

./lencod.exe -f ./tests/pedestrian_area/encoder_5_10.cfg &> ./tests/pedestrian_area/results/encoder_5_10.res
./ldecod.exe -f ./tests/pedestrian_area/decoder.cfg 
gst-launch-0.10 filesrc location=./tests/pedestrian_area/pedestrian_area_dec.yuv ! videoparse format=1 width=1920 height=1080 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/pedestrian_area/results/pedestrian_area_5_10.ogg


echo "=== Starting 10/30 test ==="

./lencod.exe -f ./tests/pedestrian_area/encoder_10_30.cfg &> ./tests/pedestrian_area/results/encoder_10_30.res
./ldecod.exe -f ./tests/pedestrian_area/decoder.cfg 
gst-launch-0.10 filesrc location=./tests/pedestrian_area/pedestrian_area_dec.yuv ! videoparse format=1 width=1920 height=1080 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/pedestrian_area/results/pedestrian_area_10_30.ogg


echo "=== Starting 10/60 test ==="

./lencod.exe -f ./tests/pedestrian_area/encoder_10_60.cfg &> ./tests/pedestrian_area/results/encoder_10_60.res
./ldecod.exe -f ./tests/pedestrian_area/decoder.cfg 
gst-launch-0.10 filesrc location=./tests/pedestrian_area/pedestrian_area_dec.yuv ! videoparse format=1 width=1920 height=1080 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! oggmux ! filesink location=./tests/pedestrian_area/results/pedestrian_area_10_60.ogg

echo "=== We are done ==="
