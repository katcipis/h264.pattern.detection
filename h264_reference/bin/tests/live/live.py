import gobject, pygtk, gtk, pygst, sys
pygst.require("0.10")
import os, gst, glib, subprocess, time


_ENCODER_TEMPLATE_FILE = "encoder.cfg.template"
_ENCODER_FILE = os.path.join (os.getcwd(), "encoder.cfg")
_INPUT_FILE = "/tmp/live_named_pipe"

_OBJECT_DETECTION_ENABLE        = 1
_OBJECT_DETECTION_MIN_HEIGHT    = 30
_OBJECT_DETECTION_MIN_WIDTH     = 30
_OBJECT_DETECTION_SEARCH_HYST   = 10
_OBJECT_DETECTION_TRACKING_HYST = 30
_OBJECT_DETECTION_TRAINING_FILE = os.path.join (os.getcwd(), "haarcascade_frontalface_alt.xml")


def get_options ():

	if len(sys.argv) < 5:
		print("usage: {0} <number of frames> <frame_rate> <width> <height> ".format(sys.argv[0]))
		exit(0)

	return sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]


def generate_encoder_configuration (frames_to_encode, frame_rate, width, height):

	config_template = open (_ENCODER_TEMPLATE_FILE, "r")

	config_file_str = config_template.read().format(InputFile1 = _INPUT_FILE, 
                                                InputFile2 = _INPUT_FILE, 
                                                FramesToBeEncoded = frames_to_encode, 
                                                FrameRate = frame_rate, 
                                                SourceWidth = width, 
                                                SourceHeight = height, 
                                                OutputWidth = width, 
                                                OutputHeight = height,
                                                object_detection_enable = _OBJECT_DETECTION_ENABLE,
                                                object_detection_min_width = _OBJECT_DETECTION_MIN_WIDTH,
                                                object_detection_min_height = _OBJECT_DETECTION_MIN_HEIGHT,
                                                object_detection_search_hysteresis = _OBJECT_DETECTION_SEARCH_HYST,
                                                object_detection_tracking_hysteresis = _OBJECT_DETECTION_TRACKING_HYST,
                                                object_detection_training_file = _OBJECT_DETECTION_TRAINING_FILE)

	config_template.close()

	config_file = open (_ENCODER_FILE, "w")
	config_file.write(config_file_str)
	config_file.close()



def gst_bus_handler (bus, message):
 	''' Bus call message handler '''
        if message.type == gst.MESSAGE_ERROR:
            err, debug = message.parse_error()
            print("Error: {0}".format(debug))
            self.__pipeline.set_state(gst.STATE_NULL)

        elif message.type == gst.MESSAGE_WARNING:
            err, debug = message.parse_warning()
            print("Warning: {0}".format(debug))

	elif message.type == gst.MESSAGE_EOS:
            print("Stopping pipeline !!!")
	    gtk.quit()

        else:
            print("Message type {0}".format(message.type))



def build_receive_pipeline(frames_to_encode, frame_rate, width, height):
	pipeline = gst.Pipeline()
        bus = pipeline.get_bus()

        bus.add_signal_watch()
        bus.enable_sync_message_emission()
        bus.connect("message", gst_bus_handler)

        src   = gst.element_factory_make("v4l2src", "src")
        colorspace = gst.element_factory_make("ffmpegcolorspace", "colorspace")
        videoscale = gst.element_factory_make("videoscale", "video-scale")
        videorate  = gst.element_factory_make("videorate", "video-rate")
        caps       = gst.element_factory_make("capsfilter", "video-caps")
        queue = gst.element_factory_make("queue", "buffer")
        sink  = gst.element_factory_make("filesink", "file-sink")
	video_caps = "video/x-raw-yuv,format=\(fourcc\)I420,width={width},height={height},framerate={framerate}".format (width = width, 
                                                                                                                         height = height, 
                                                                                                                         framerate = frame_rate)
        sink.set_property ("location", _INPUT_FILE)
        sink.set_property ("num-buffers", frames_to_encode)
	caps.set_property("caps", gst.caps_from_string(video_caps))
        
        pipeline.add (src, colorspace, videoscale, videorate, caps, queue, sink)
	gst.element_link_many(src, queue, sink)
	
	return pipeline


generate_encoder_configuration (*get_options())

capture_pipeline = build_receive_pipeline(*get_options())
capture_pipeline.set_state (gst.STATE_PLAYING)

gtk.gdk.threads_init()
gtk.main()

subprocess.call ("../../lencod.exe -f " + _ENCODER_FILE, shell=True)

