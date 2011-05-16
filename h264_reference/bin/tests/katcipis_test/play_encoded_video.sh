gst-launch-0.10 -v filesrc location=katcipis_sif.h264 ! decodebin ! ffmpegcolorspace ! videoparse format=1 width=352 height=288 framerate=30/1 ! ffmpegcolorspace ! videorate ! autovideosink
