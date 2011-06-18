wget http://media.xiph.org/video/derf/y4m/coastguard_qcif.y4m
gst-launch -v filesrc location=coastguard_qcif.y4m ! decodebin2 ! filesink location=coast_guard_qcif.yuv
rm coastguard_qcif.y4m
