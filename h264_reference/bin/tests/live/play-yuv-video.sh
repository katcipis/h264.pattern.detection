gst-launch-0.10 filesrc location=$1 ! videoparse format=1 width=$2 height=$3 framerate=$4 ! ffmpegcolorspace ! autovideosink
