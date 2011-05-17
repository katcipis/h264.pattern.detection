gst-launch-0.10 -v filesrc location=shields_720p.h264 ! decodebin ! ffmpegcolorspace ! videoparse format=1 width=1280 height=720 framerate=50/1 ! ffmpegcolorspace ! videorate ! autovideosink
