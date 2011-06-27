#!/bin/bash

file pedestrian_area.yuv

if test ! $? -eq 0; then
    file pedestrian_area_1080p25.y4m
    if test ! $? -eq 0; then
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
nice -n -20 ./lencod.exe -f ./tests/pedestrian_area/encoder.cfg &> ./tests/pedestrian_area/results/encoder_original.res


echo "=== Starting 1/1 test ==="

nice -n -20 ./lencod.exe -f ./tests/pedestrian_area/encoder_1_1.cfg &> ./tests/pedestrian_area/results/encoder_1_1.res
nice -n -20 ./ldecod.exe -f ./tests/pedestrian_area/decoder.cfg
nice -n -20 gst-launch-0.10 filesrc location=./tests/pedestrian_area/pedestrian_area_dec.yuv ! videoparse format=1 width=1920 height=1080 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! avimux ! filesrc location=./tests/pedestrian_area/results/pedestrian_area_1_1.avi


echo "=== Starting 5/10 test ==="

nice -n -20 ./lencod.exe -f ./tests/pedestrian_area/encoder_5_10.cfg &> ./tests/pedestrian_area/results/encoder_5_10.res
nice -n -20 ./ldecod.exe -f ./tests/pedestrian_area/decoder.cfg 
nice -n -20 gst-launch-0.10 filesrc location=./tests/pedestrian_area/pedestrian_area_dec.yuv ! videoparse format=1 width=1920 height=1080 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! avimux ! filesrc location=./tests/pedestrian_area/results/pedestrian_area_5_10.avi


echo "=== Starting 10/30 test ==="

nice -n -20 ./lencod.exe -f ./tests/pedestrian_area/encoder_10_30.cfg &> ./tests/pedestrian_area/results/encoder_10_30.res
nice -n -20 ./ldecod.exe -f ./tests/pedestrian_area/decoder.cfg 
nice -n -20 gst-launch-0.10 filesrc location=./tests/pedestrian_area/pedestrian_area_dec.yuv ! videoparse format=1 width=1920 height=1080 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! avimux ! filesrc location=./tests/pedestrian_area/results/pedestrian_area_10_30.avi


echo "=== Starting 10/60 test ==="

nice -n -20 ./lencod.exe -f ./tests/pedestrian_area/encoder_10_60.cfg &> ./tests/pedestrian_area/results/encoder_10_60.res
nice -n -20 ./ldecod.exe -f ./tests/pedestrian_area/decoder.cfg 
nice -n -20 gst-launch-0.10 filesrc location=./tests/pedestrian_area/pedestrian_area_dec.yuv ! videoparse format=1 width=1920 height=1080 framerate=25/1 ! ffmpegcolorspace ! theoraenc ! avimux ! filesrc location=./tests/pedestrian_area/results/pedestrian_area_10_60.avi

echo "=== We are done ==="
