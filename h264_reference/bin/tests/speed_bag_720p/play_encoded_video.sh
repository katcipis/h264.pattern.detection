gst-launch-0.10 -v filesrc location=speed_bag.h264 ! decodebin2 ! ffmpegcolorspace ! videorate ! autovideosink
