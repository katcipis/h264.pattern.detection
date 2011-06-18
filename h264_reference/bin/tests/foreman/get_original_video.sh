wget http://media.xiph.org/video/derf/y4m/foreman_cif.y4m
gst-launch -v filesrc location=foreman_cif.y4m ! decodebin2 ! filesink location=foreman.yuv
rm foreman_cif.y4m
