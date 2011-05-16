gst-launch-0.10 filesrc location=./katcipis_sif.yuv ! videoparse format=1 width=352 height=288 framerate=30/1 ! ffmpegcolorspace ! autovideosink
