gst-launch-0.10 -v filesrc location=pedestrian_area.h264 ! decodebin2 ! ffmpegcolorspace ! videorate ! autovideosink
