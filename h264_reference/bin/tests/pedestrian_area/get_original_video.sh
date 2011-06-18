wget http://media.xiph.org/video/derf/y4m/pedestrian_area_1080p25.y4m
gst-launch -v filesrc location=pedestrian_area_1080p25.y4m ! decodebin2 ! filesink location=pedestrian_area.yuv
rm pedestrian_area_1080p25.y4m
