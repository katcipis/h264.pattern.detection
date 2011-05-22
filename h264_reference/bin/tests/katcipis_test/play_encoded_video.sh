gst-launch-0.10 -v filesrc location=katcipis_720p.h264 ! decodebin ! ffmpegcolorspace ! videoparse format=1 width=1280 height=720 framerate=60 ! ffmpegcolorspace ! videorate ! autovideosink
