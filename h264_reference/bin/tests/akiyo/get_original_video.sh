wget http://media.xiph.org/video/derf/y4m/akiyo_qcif.y4m
gst-launch -v filesrc location=akiyo_qcif.y4m ! decodebin2 ! filesink location=akiyo_qcif.yuv
rm akiyo_qcif.y4m
