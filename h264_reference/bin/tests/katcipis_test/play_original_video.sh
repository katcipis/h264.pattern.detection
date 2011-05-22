sudo nice --20 gst-launch-0.10 filesrc location=./katcipis_720p.yuv ! queue ! videoparse format=1 width=1280 height=720 framerate=50 ! videorate ! ffmpegcolorspace ! autovideosink
