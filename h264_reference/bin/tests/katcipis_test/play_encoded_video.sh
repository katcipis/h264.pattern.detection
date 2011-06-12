gst-launch-0.10 -v filesrc location=katcipis.h264 ! decodebin ! ffmpegcolorspace ! videoparse format=1 width=640 height=360 framerate=30/1 ! ffmpegcolorspace ! videorate ! autovideosink
