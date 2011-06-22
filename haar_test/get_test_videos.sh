wget http://media.xiph.org/video/derf/y4m/crowd_run_1080p50.y4m

echo ""
echo "Generating 1080p YUV 4:2:0 video"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! filesink location=crowd_run_1080p.yuv

echo ""
echo "Generating 720p YUV 4:2:0 video"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=1280,height=720 ! filesink location=crowd_run_720p.yuv

echo ""
echo "Generating 640 x 360 YUV 4:2:0 video"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=640,height=360 ! filesink location=crowd_run_640_360.yuv

echo ""
echo "Generating 320 x 180 YUV 4:2:0 video"
echo ""

gst-launch -v filesrc location=crowd_run_1080p50.y4m ! decodebin2 ! videoscale ! video/x-raw-yuv,width=320,height=180 ! filesink location=crowd_run_320_180.yuv
