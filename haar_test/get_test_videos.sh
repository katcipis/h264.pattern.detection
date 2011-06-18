# wget http://media.xiph.org/video/derf/y4m/pedestrian_area_1080p25.y4m

echo ""
echo "Generating 1080p RGB 24 bits bpp video"
echo ""

gst-launch -v filesrc location=pedestrian_area_1080p25.y4m ! decodebin2 ! ffmpegcolorspace ! videoscale ! video/x-raw-rgb,bpp=24,depth=24 ! filesink location=pedestrian_area.yuv

