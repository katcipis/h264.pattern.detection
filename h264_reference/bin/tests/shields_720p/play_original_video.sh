gst-launch-0.10 filesrc location=./shields_720p.yuv ! queue ! videoparse format=1 width=1280 height=720 framerate=50/1 ! ffmpegcolorspace ! autovideosink
