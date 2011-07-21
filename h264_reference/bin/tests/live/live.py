import gobject, pygst, sys
pygst.require("0.10")
import os, gst, glib


_ENCODER_TEMPLATE_FILE = "encoder.cfg.template"
_ENCODER_FILE = "encoder.cfg"
_INPUT_FILE = "/tmp/live_named_pipe"

_OBJECT_DETECTION_ENABLE        = 1
_OBJECT_DETECTION_MIN_HEIGHT    = 30
_OBJECT_DETECTION_MIN_WIDTH     = 30
_OBJECT_DETECTION_SEARCH_HYST   = 10
_OBJECT_DETECTION_TRACKING_HYST = 30
_OBJECT_DETECTION_TRAINING_FILE = "haarcascade_frontalface_alt.xml"


def get_options ():

	if len(sys.argv) < 5:
		print("usage: {0} <number of frames> <frame_rate> <width> <height>".format(sys.argv[0]))
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


generate_encoder_configuration (*get_options())

