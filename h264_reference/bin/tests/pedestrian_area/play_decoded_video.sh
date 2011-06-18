gst-launch-0.10 filesrc location=./foreman_dec.yuv ! videoparse format=1 width=352 height=288 framerate=30/1 ! ffmpegcolorspace ! autovideosink
