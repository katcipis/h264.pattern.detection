gst-launch-0.10 filesrc location=./katcipis.yuv ! queue ! videoparse format=1 width=640 height=360 framerate=30/1 ! videorate ! ffmpegcolorspace ! autovideosink
