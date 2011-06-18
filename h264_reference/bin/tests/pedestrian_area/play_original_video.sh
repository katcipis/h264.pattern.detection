gst-launch-0.10 filesrc location=./pedestrian_area.yuv ! videoparse format=1 width=1920 height=1080 framerate=25/1 ! ffmpegcolorspace ! autovideosink
