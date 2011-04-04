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
static CvMemStorage * storage    = NULL;

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

void metadata_extractor_free_all_extracted_metadata(ExtractedMetadata ** metadata_objs)
{
  ExtractedMetadata ** tmp = metadata_objs;

  while (*tmp) {
    extracted_metadata_free(*tmp);
    ++tmp;
  }

  free(metadata_objs);
}

/*!
 *************************************************************************************
 * \brief
 *    Function body for extract metadata from the y image plane.
 *
 * \return
 *    A ExtractedMetadata object array (NULL terminated) or NULL in case no metadata is extracted.
 *
 *************************************************************************************
 */
ExtractedMetadata ** metadata_extractor_extract_from_y(unsigned char ** y, int width, int height)
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
      return NULL;
  }

  metadata_objs = malloc(sizeof(ExtractedMetadata *) * (results->total + 1));

  /* On this case we will sent the grayscale found object as metadata (uncompressed) */

  for( i = 0; i < results->total; i++ ) {
  
    CvRect* res                = (CvRect*)cvGetSeqElem( results, i);
    ExtractedYImage * metadata = extracted_y_image_new(res->width, res->height);
    unsigned char ** y_plane   = extracted_y_image_get_y(metadata);
    int metadata_row           = 0;
   
    /* Copy the object from the original frame */
    for (row = res->y; row < (res->height + res->y); row++) {

      int metadata_col = 0;

      for (col = res->x; col < (res->width + res->x); col ++) {
          y_plane[metadata_row][metadata_col] = y[row][col];
          metadata_col++;
      }
      metadata_row++;
    }

    metadata_objs[i] = (ExtractedMetadata *) metadata;
  }
 
  /* NULL termination */
  metadata_objs[results->total] = NULL;

  /* Freeing images */
  cvReleaseImage(&frame);
  cvReleaseImage(&gray);

  return metadata_objs;
}

