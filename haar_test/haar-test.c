/*
* This test simply process a raw RGB video on OpenCV Haar.
* It is usefull to measure the quality of openCV using different video configurations (resolution, quality, etc).
* Haar is configured here the same way it is configured inside the MetadataExtractor used on the H.264 encoder.
* It will not show the results, it will only report how much interest objects have been found.
* 
* @author Tiago Katcipis <tiagokatcipis@gmail.com>. 
*
*/
#include <cv.h>	      /* for OpenCV basic structures for ex.: IPLImage */
#include <highgui.h>  /* for OpenCV functions like cvCvtColor */
#include <stdio.h>

int main(int argc, char **argv)
{
  int bytedepth           = 0;
  int width               = 0;
  int height              = 0;
  int total_objects_found = 0;
  FILE * input_video_file = NULL;

  if (argc < 6) {
    printf("Usage: %s <haar training filename> <video file name> <bit depth> <width> <height>\n", argv[0]);
    return -1;
  }

  bytedepth = atoi(argv[3]);
  width     = atoi(argv[4]);
  height    = atoi(argv[5]);

  input_video_file = fopen(argv[2], "r");

  fread(void *ptr, size_t size, size_t nmemb, input_video_file);

  return 0;
}

