/*
* This test simply process a raw BGR video on OpenCV Haar, The bit depth must be from 8, 16 or 32 bits (Integer).
* It is usefull to measure the quality of openCV using different video configurations (resolution, quality, etc).
* Haar is configured here the same way it is configured inside the MetadataExtractor used on the H.264 encoder.
* It will not show the results, it will only report how much interest objects have been found.
* 
* @author Tiago Katcipis <tiagokatcipis@gmail.com>. 
*
*/
#include <cv.h>	      /* for OpenCV basic structures for ex.: IPLImage */
#include <highgui.h>  /* for OpenCV functions like cvCvtColor */
#include <glib.h>
#include <stdio.h>


static const int NUM_CHANNELS      = 3;
static const int MIN_OBJECT_WIDTH  = 30;
static const int MIN_OBJECT_HEIGHT = 30;


int get_bit_depth(int bitdepth)
{
  switch (bitdepth){
  
    case 8:
      return IPL_DEPTH_8U;

    case 16:
      return IPL_DEPTH_16S;

    case 32:
      return IPL_DEPTH_32S;

    default:
      printf("get_bit_depth: Unsupported bitdepth[%d] !!!\n", bitdepth);
      exit(-1);
  }
}


int main(int argc, char **argv)
{
  /* OpenCV Haar config, its the same used on the H.264 encoder */
  CvHaarClassifierCascade * classifier = NULL;
  CvMemStorage * storage = NULL;
  double scale_factor    = 1.1f;
  int min_neighbors     = 3;
  int haar_flags         = CV_HAAR_FIND_BIGGEST_OBJECT | CV_HAAR_DO_ROUGH_SEARCH | CV_HAAR_DO_CANNY_PRUNING; 
  CvSize min_size;
 
  /* Test stuff */
  IplImage * image        = NULL;
  IplImage * gray_image   = NULL;
  GTimer * timer          = NULL;
  FILE * input_video_file = NULL;
  int bytedepth           = 0;
  int total_objects_found = 0;  
  int height              = 0;
  int width               = 0;
  int frame_size          = 0;
  int total_frames        = 0;
  gdouble min_elapsed     = G_MAXDOUBLE;
  gdouble max_elapsed     = 0;
  gdouble total_elapsed   = 0;


  if (argc < 6) {
    printf("Usage: %s <haar training filename> <video file name> <bit depth> <width> <height>\n", argv[0]);
    return -1;
  }

  bytedepth = atoi(argv[3]) / 8;
  width     = atoi(argv[4]);
  height    = atoi(argv[5]);

  input_video_file = fopen(argv[2], "r");

  image            = cvCreateImage(cvSize(width, height), get_bit_depth(atoi(argv[3])), NUM_CHANNELS);
  gray_image       = cvCreateImage(cvSize(width, height), get_bit_depth(atoi(argv[3])), 1);

  min_size.width   = MIN_OBJECT_WIDTH;
  min_size.height  = MIN_OBJECT_HEIGHT;

  printf("\nStarting Haar test, bytedepth[%d] width[%d] height[%d] object min size[%d][%d] scale factor[%f] min_neighbors[%d]\n\n", 
         bytedepth, width, height, min_size.height, min_size.width, scale_factor, min_neighbors);

  frame_size = width * height * NUM_CHANNELS;
  storage    = cvCreateMemStorage(0);
  classifier = (CvHaarClassifierCascade*) cvLoad(argv[1], 0, 0, 0 );
  timer      = g_timer_new();

  while (fread(image->imageData, bytedepth, frame_size, input_video_file) == frame_size) {
    CvSeq* results  = NULL;
    gdouble elapsed = 0; 

    cvCvtColor(image, gray_image, CV_BGR2GRAY);

    /* cvEqualizeHist spreads out the brightness values necessary because the integral image 
     features are based on differences of rectangle regions and, if the histogram is not balanced, 
     these differences might be skewed by overall lighting or exposure of the test images. */
    cvEqualizeHist(gray_image, gray_image);

    g_timer_start(timer);

    results =  cvHaarDetectObjects (gray_image,
                                    classifier,
                                    storage,
                                    scale_factor,
                                    min_neighbors,
                                    haar_flags,
                                    min_size);

    elapsed = g_timer_elapsed (timer, NULL);

    if (elapsed < min_elapsed) {
      min_elapsed = elapsed 
    }
 
    if (elapsed > max_elapsed) { 
      max_elapsed = max_elapsed;
    }

    total_elapsed += elapsed;
    total_objects_found += results->total;
    total_frames++;
    /* Results belongs to the storage, it will be freed later */
  }

  if (!feof(input_video_file)) {
    printf(">>>>>>> An error occured while reading the frames from the video file !!!\n");
  }

  cvReleaseImage(&image);
  cvReleaseImage(&gray_image);
  cvClearMemStorage(storage);
  fclose(input_video_file);

  printf("\n\n====================================================================================================\n");
  printf("Identified [%d] objects on a video with [%d] frames \n", total_objects_found, total_frames);
  printf("Haar profiling (seconds): min elapsed[%f] max elapsed[%f] total elapsed[%f] mean elapsed[%f]\n", 
         min_elapsed, max_elapsed, total_elapsed, total_elapsed / total_frames);
  printf("========================================================================================================\n\n");

  return 0;
}

