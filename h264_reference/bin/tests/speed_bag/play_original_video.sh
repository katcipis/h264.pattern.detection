gst-launch-0.10 filesrc location=./speed_bag.yuv ! videoparse format=1 width=1920 height=1080 framerate=30/1 ! ffmpegcolorspace ! autovideosink
