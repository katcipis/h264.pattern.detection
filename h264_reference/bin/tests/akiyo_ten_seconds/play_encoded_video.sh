gst-launch-0.10 -v filesrc location=akiyo_qcif.h264 ! decodebin ! ffmpegcolorspace ! videoparse format=1 width=176 height=144 framerate=30/1 ! ffmpegcolorspace ! videorate ! autovideosink
