#include "metadata_extractor.h"
#include <cv.h>
#include <cvaux.h>
#include <highgui.h>
#include <stdio.h>


/* Filename of the xml defining the trained Haar cascade */
static const char * cascade_filename = "haarcascade_frontalface_alt.xml";

/* Haar feature cascade */
static CvHaarClassifierCascade * classifier = NULL;

/* OpenCV “work buffer" */
static CvMemStorage * storage      = NULL;

/* How big of a jump there is between each scale */
static const double SCALE_FACTOR = 1.1f;

/* only decide a face is present in a location if there are at least three overlapping detections. */
static const int MIN_NEIGHBORS = 3;

/* Minimum object size */  
static CvSize MIN_SIZE = {30, 30};


static void init_haar_facilities()
{
  if (!classifier) {
    classifier = (CvHaarClassifierCascade*) cvLoad( cascade_filename, 0, 0, 0 );
  }

  if (!storage) {
    storage = cvCreateMemStorage(0);
  }

  /* prepares memory for the classifier (if it was used before already) */
  cvClearMemStorage(storage);
}

/*!
 *************************************************************************************
 * \brief
 *    Function body for extract metadata from a yuv image.
 *
 * \return
 *    The ExtractedMetadata object or NULL.
 *
 *************************************************************************************
 */
ExtractedMetadata ** metadata_extractor_extract_from_yuv(unsigned char ** y, int width, int height)
{
  /* First we must convert the Y luma plane to BGR and them to grayscale. 
     On grayscale Y = R = G = B. Pretty simple to convert. */

  IplImage * frame                   = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
  IplImage * gray                    = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  CvSeq* results                     = NULL;
  ExtractedMetadata ** metadata_objs = NULL;

  int frame_i = 0;
  int row     = 0;
  int col     = 0;
  int i;

  /* We must write R G B with the same Y sample*/
  for (row = 0; row < height; row++) {

    for (col = 0; col < width; col++) {

      frame->imageData[frame_i]     = y[row][col];
      frame->imageData[frame_i + 1] = y[row][col];
      frame->imageData[frame_i + 2] = y[row][col];
      frame_i += 3;
    }
  }

  cvCvtColor( frame, gray, CV_BGR2GRAY );

  /* cvEqualizeHist spreads out the brightness values necessary because the integral image 
     features are based on differences of rectangle regions and, if the histogram is not balanced, 
     these differences might be skewed by overall lighting or exposure of the test images. */
  cvEqualizeHist(gray, gray);

  
  /* Lets start detection */
  init_haar_facilities();

  results =  cvHaarDetectObjects (gray, 
                                  classifier, 
                                  storage, 
                                  SCALE_FACTOR, 
                                  MIN_NEIGHBORS, 
                                  CV_HAAR_DO_CANNY_PRUNING, /* skip flat regions */ 
                                  MIN_SIZE);

  if (results && (results->total <= 0) ) {
      printf("Cant extract metadata from this frame \n");
      return NULL;
  }

  printf("Found [%d] metadata objects !!!\n", results->total);

  metadata_objs = malloc(sizeof(ExtractedMetadata *) * (results->total + 1));

  /* On this case we will sent the grayscale found object as metadata (uncompressed) */

  for( i = 0; i < results->total; i++ ) {
  
    ExtractedMetadata * metadata = malloc(sizeof(ExtractedMetadata));
    CvRect* res                  = (CvRect*)cvGetSeqElem( results, i);
 
    /* Allocate some space for the object */
    metadata->height = res->height;
    metadata->width  = res->width;
    metadata->y      = malloc(res->height);

    /* Copy the object from the original frame */
    for (row = res->y; row < (res->height + res->y); row++) {

      metadata->y[row] = malloc(res->width);
      memcpy (metadata->y[row], y[row] + res->x, res->width);

    }

    metadata_objs[i] = metadata;
  }
 
  /* NULL termination */
  metadata_objs[results->total = 1] = NULL;

  /*
  cvNamedWindow("frame", 1);
  cvShowImage("frame", frame);
  cvWaitKey(1000);
  */

  return metadata_objs;
}

