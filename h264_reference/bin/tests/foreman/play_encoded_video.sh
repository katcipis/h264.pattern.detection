gst-launch-0.10 -v filesrc location=foreman.h264 ! decodebin ! ffmpegcolorspace ! videoparse format=1 width=385 height=288 framerate=30/1 ! ffmpegcolorspace ! videorate ! autovideosink
