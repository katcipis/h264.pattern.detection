gst-launch-0.10 filesrc location=./speed_bag_dec.yuv ! videoparse format=1 width=1280 height=720 framerate=30/1 ! ffmpegcolorspace ! autovideosink
