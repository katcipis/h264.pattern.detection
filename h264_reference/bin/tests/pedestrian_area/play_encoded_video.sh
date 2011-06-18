gst-launch-0.10 -v filesrc location=foreman.h264 ! decodebin2 ! ffmpegcolorspace ! videorate ! autovideosink
