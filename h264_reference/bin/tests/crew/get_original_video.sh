wget http://media.xiph.org/video/derf/y4m/crew_cif.y4m
gst-launch -v filesrc location=crew_cif.y4m ! decodebin2 ! filesink location=crew_cif.yuv
rm crew_cif.y4m
