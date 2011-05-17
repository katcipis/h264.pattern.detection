echo "Downloading file"
wget http://media.xiph.org/video/derf/y4m/720p/720p50_shields_ter.y4m

echo "Removing y4m header"
gst-launch -v filesrc location=720p50_shields_ter.y4m ! decodebin ! filesink location=shields_720p.yuv

echo "We are done :-)"
