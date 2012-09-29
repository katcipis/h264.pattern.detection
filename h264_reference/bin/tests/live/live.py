#!/usr/bin/env python

import gobject, pygtk, gtk, pygst, sys
pygst.require("0.10")
import os, gst, glib, subprocess, time


_TMP_FILES_DIRECTORY = os.path.join(os.getcwd(), "tmp")

_ENCODER_TEMPLATE_FILE = os.path.join (os.getcwd(), "encoder.cfg.template")
_DECODER_TEMPLATE_FILE = os.path.join (os.getcwd(), "decoder.cfg.template")

_ENCODER_FILE = os.path.join (os.getcwd(), "encoder.cfg")
_DECODER_FILE = os.path.join (os.getcwd(), "decoder.cfg")

_ENCODER_INPUT_FILE  = os.path.join(_TMP_FILES_DIRECTORY, "live-recorded.yuv")
_ENCODER_OUTPUT_FILE = os.path.join(_TMP_FILES_DIRECTORY, "live-encoded.h264")
_DECODER_INPUT_FILE  = _ENCODER_OUTPUT_FILE
_DECODER_OUTPUT_FILE = os.path.join(_TMP_FILES_DIRECTORY, "live-recorded-decoded.yuv")
_DECODER_REFERENCE_FILE = _ENCODER_INPUT_FILE

_OBJECT_DETECTION_ENABLE        = 1
_OBJECT_DETECTION_MIN_HEIGHT    = 30
_OBJECT_DETECTION_MIN_WIDTH     = 30
_OBJECT_DETECTION_SEARCH_HYST   = 10
_OBJECT_DETECTION_TRACKING_HYST = 30
_OBJECT_DETECTION_TRAINING_FILE = os.path.join(os.getcwd(), "..", "..", "haarcascade_frontalface_alt.xml")



def get_options ():

    if len(sys.argv) < 5:
        print("usage: {0} <number of frames> <frame_rate> <width> <height> ".format(sys.argv[0]))
        print("some other configurations can be done directly inside the {0} file".format(sys.argv[0]))
	exit(0)

    return sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]



def generate_decoder_configuration ():

    config_template = open (_DECODER_TEMPLATE_FILE, "r")
  
    config_file_str = config_template.read().format(InputFile = _DECODER_INPUT_FILE,
                                                    OutputFile = _DECODER_OUTPUT_FILE,
                                                    RefFile = _DECODER_REFERENCE_FILE)

    config_template.close()

    config_file = open (_DECODER_FILE, "w")
    config_file.write(config_file_str)
    config_file.close()


def generate_encoder_configuration (frames_to_encode, frame_rate, width, height):

	config_template = open (_ENCODER_TEMPLATE_FILE, "r")
  
	config_file_str = config_template.read().format(InputFile1 = _ENCODER_INPUT_FILE, 
                                                        InputFile2 = _ENCODER_INPUT_FILE, 
                                                        FramesToBeEncoded = frames_to_encode, 
                                                        FrameRate = frame_rate, 
                                                        SourceWidth = width, 
                                                        SourceHeight = height, 
                                                        OutputWidth = width, 
                                                        OutputHeight = height,
                                                        OutputFile = _ENCODER_OUTPUT_FILE,
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
            exit()

        elif message.type == gst.MESSAGE_WARNING:
            err, debug = message.parse_warning()
            print("Warning: {0}".format(debug))

	elif message.type == gst.MESSAGE_EOS:
            print("EOS: stopping pipeline !!!")
	    gtk.main_quit()

        else:
            print("Message type {0}".format(message.type))


def build_yuv_420_videoparse(width, height, frame_rate):
    videoparse = gst.element_factory_make("videoparse", "video-caps")

    videoparse.set_property("format", 1)
    videoparse.set_property("width", int(width))
    videoparse.set_property("height", int(height))
    videoparse.set_property("framerate", gst.Fraction(int(frame_rate), 1))
    return videoparse


def build_capture_pipeline(frames_to_encode, frame_rate, width, height):

    pipeline = gst.Pipeline()
    bus = pipeline.get_bus()

    bus.add_signal_watch()
    bus.enable_sync_message_emission()
    bus.connect("message", gst_bus_handler)

    #src   = gst.element_factory_make("v4l2src", "src") 
    src   = gst.element_factory_make("videotestsrc", "src")
    src.set_property("is-live", True)

    colorspace = gst.element_factory_make("ffmpegcolorspace", "colorspace")
    videoscale = gst.element_factory_make("videoscale", "video-scale")
    videorate  = gst.element_factory_make("videorate", "video-rate")
    videoparse = build_yuv_420_videoparse(width, height, frame_rate)
    tee        = gst.element_factory_make("tee", "videotee")
    tee_queue1 = gst.element_factory_make("queue", "tee-queue1")
    tee_queue2 = gst.element_factory_make("queue", "tee-queue2")
    filesink   = gst.element_factory_make("filesink", "file-sink")
    videosink  = gst.element_factory_make("autovideosink", "video-sink")

    filesink.set_property ("location", _ENCODER_INPUT_FILE)
    src.set_property ("num-buffers", int(frames_to_encode))

    pipeline.add (src, colorspace, videoscale, videorate, videoparse, tee_queue1, tee_queue2, tee, videosink, filesink)
    gst.element_link_many(src, colorspace, videoscale, videorate, videoparse, tee)
    gst.element_link_many(tee, tee_queue1, filesink)
    gst.element_link_many(tee, tee_queue2, videosink)

    return pipeline



def build_playback_pipeline(frames_to_encode, frame_rate, width, height):
    pipeline = gst.Pipeline()
    bus = pipeline.get_bus()

    bus.add_signal_watch()
    bus.enable_sync_message_emission()
    bus.connect("message", gst_bus_handler)

    src   = gst.element_factory_make("filesrc", "src")
    videoparse = build_yuv_420_videoparse(width, height, frame_rate)
    colorspace = gst.element_factory_make("ffmpegcolorspace", "colorspace")
    videoscale = gst.element_factory_make("videoscale", "video-scale")
    videorate  = gst.element_factory_make("videorate", "video-rate")
    queue = gst.element_factory_make("queue", "buffer")
    sink  = gst.element_factory_make("autovideosink", "video-sink")

    src.set_property ("location", _DECODER_OUTPUT_FILE)

    pipeline.add (src, videoparse, colorspace, videoscale, videorate, queue, sink)
    gst.element_link_many(src, videoparse, colorspace, videoscale, videorate, queue, sink)

    return pipeline


if not os.path.isdir(_TMP_FILES_DIRECTORY) :
    os.mkdir(_TMP_FILES_DIRECTORY)


generate_encoder_configuration (*get_options())
generate_decoder_configuration ()

capture_pipeline = build_capture_pipeline(*get_options())
capture_pipeline.set_state (gst.STATE_PLAYING)

gtk.gdk.threads_init()
gtk.main()

capture_pipeline.set_state (gst.STATE_NULL)

subprocess.call (os.path.join("..", "..","lencod.exe") + " -f " + _ENCODER_FILE, shell=True)
subprocess.call (os.path.join("..", "..", "ldecod.exe") + " -f " + _DECODER_FILE, shell=True)

play_pipeline = build_playback_pipeline(*get_options())
play_pipeline.set_state (gst.STATE_PLAYING)

gtk.main()

play_pipeline.set_state (gst.STATE_NULL)

