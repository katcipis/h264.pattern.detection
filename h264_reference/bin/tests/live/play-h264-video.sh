gst-launch-0.10 -v filesrc location=$1 ! decodebin ! ffmpegcolorspace ! videorate ! autovideosink

