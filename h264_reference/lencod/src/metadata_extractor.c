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


static CvRect* metadata_extractor_search_for_object_of_interest(unsigned char ** y, int width, int height)
{
  IplImage * frame                   = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
  IplImage * gray                    = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  CvSeq* results                     = NULL;

  int frame_i = 0;
  int row     = 0;
  int col     = 0;

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

  /* The search is optimized to find only one object */
  results =  cvHaarDetectObjects (gray,
                                  classifier,
                                  storage,
                                  SCALE_FACTOR,
                                  MIN_NEIGHBORS,
                                  CV_HAAR_FIND_BIGGEST_OBJECT | 
                                  CV_HAAR_DO_ROUGH_SEARCH | 
                                  CV_HAAR_DO_CANNY_PRUNING, 
                                  MIN_SIZE);

  /* Freeing images */
  cvReleaseImage(&frame);
  cvReleaseImage(&gray);

  if (results && (results->total <= 0) ) {
      return NULL;
  }

  return (CvRect*)cvGetSeqElem( results, 0);
}


/*!
 *************************************************************************************
 * \brief
 *    Function body for extract metadata from the y image plane.
 *
 * \return
 *    A ExtractedMetadata object or NULL in case no metadata is extracted.
 *
 *************************************************************************************
 */
ExtractedMetadata * metadata_extractor_extract_raw_object(unsigned int frame_num, 
                                                          unsigned char ** y, 
                                                          int width, 
                                                          int height)
{
  /* First we must convert the Y luma plane to BGR and them to grayscale. 
     On grayscale Y = R = G = B. Pretty simple to convert. */

  ExtractedYImage * metadata = NULL;
  CvRect* res                = metadata_extractor_search_for_object_of_interest(y, width, height);

  if (!res) {
      return NULL;
  }

  metadata = extracted_y_image_new(frame_num, res->width, res->height);

  unsigned char ** y_plane  = extracted_y_image_get_y(metadata);
  int metadata_row          = 0;
  int row                   = 0;
  int col                   = 0;

  /* Copy the object from the original frame */
  for (row = res->y; row < (res->height + res->y); row++) {

    int metadata_col = 0;

    for (col = res->x; col < (res->width + res->x); col ++) {
        y_plane[metadata_row][metadata_col] = y[row][col];
        metadata_col++;
    }
    metadata_row++;
  }

  return (ExtractedMetadata *) metadata;
}

/******************************** 
 * ObjectBoundingBox facilities * 
 ********************************/

/* Private structs */
typedef struct _TrackedObj {
  unsigned int id;
  short x;
  short y;
  short width;
  short height;
  int motion_x;
  int motion_y;
  int motion_samples;
} TrackedObj;


/* Private const data */
static const int BLOCK_SIZE = 4;

/* Private data */
static TrackedObj* tracked_obj = NULL;


/* Private TrackedObj functions */
static TrackedObj * metadata_extractor_tracked_obj_new(CvRect* area)
{
  static unsigned int tracked_obj_id = 0;

  TrackedObj * obj = malloc(sizeof(TrackedObj));

  if (!obj) {
    return NULL;
  }

  obj->x              = area->x;
  obj->y              = area->y;
  obj->width          = area->width;
  obj->height         = area->height;
  obj->motion_x       = 0;
  obj->motion_y       = 0;
  obj->motion_samples = 0;
  obj->id             = tracked_obj_id;

  tracked_obj_id++;
  return obj;
}

static void metadata_extractor_tracked_obj_free(TrackedObj * obj)
{
  free(obj);
}

static void metadata_extractor_tracked_obj_estimate_motion(TrackedObj * obj) 
{
  if (obj->motion_samples == 0) {
    printf("metadata_extractor_tracked_obj_estimate_motion: we have no motion estimation info !!\n");
    return;
  }

  printf("metadata_extractor_tracked_obj_estimate_motion: motion_x[%d] motion_y[%d] motion_samples[%d]\n",
         obj->motion_x, obj->motion_y, obj->motion_samples);
  /* A simple arithmetic mean of all the vectors */
  printf("metadata_extractor_tracked_obj_estimate_motion: x_mov[%d] y_mov[%d]\n", 
         obj->motion_x / obj->motion_samples, obj->motion_y / obj->motion_samples);

  obj->x -= obj->motion_x / obj->motion_samples;
  obj->y -= obj->motion_y / obj->motion_samples;

  obj->motion_x       = 0;
  obj->motion_y       = 0;
  obj->motion_samples = 0;
}

static int metadata_extractor_tracked_obj_block_is_inside(short x, short y, TrackedObj * obj)
{
  /* Not going to intersect the block area with the object area, for simplicity */

  if ( (x < obj->x) || (x > obj->x + obj->width)) {
    /* x is out */
    return 0;
  }

  if ( (y < obj->y) || (y > obj->y + obj->height)) {
    /* y is out */
    return 0;
  }
  
  return 1;
}

/* Public API */
ExtractedMetadata * metadata_extractor_extract_object_bounding_box(unsigned int frame_num, 
                                                                   unsigned char ** y,
                                                                   int width,
                                                                   int height)
{
  CvRect * rect = NULL;

  if (tracked_obj) {
    /* We are tracking - Since the tracking started on the previous frame, 
       by now we have a full motion estimation for the object */
    metadata_extractor_tracked_obj_estimate_motion(tracked_obj);
    return (ExtractedMetadata *) extracted_object_bounding_box_new(tracked_obj->id, 
                                                                   frame_num, 
                                                                   tracked_obj->x, 
                                                                   tracked_obj->y, 
                                                                   tracked_obj->width, 
                                                                   tracked_obj->height);
  }
  
  rect = metadata_extractor_search_for_object_of_interest(y, width, height);

  if (!rect) {
    /* no interest object */
    return NULL;
  }

  tracked_obj = metadata_extractor_tracked_obj_new(rect); 
  return (ExtractedMetadata *) extracted_object_bounding_box_new(tracked_obj->id, 
                                                                 frame_num, 
                                                                 tracked_obj->x, 
                                                                 tracked_obj->y, 
                                                                 tracked_obj->width, 
                                                                 tracked_obj->height);
}

/***************************** 
 * ObjectTracking facilities * 
 *****************************/
void metadata_extractor_add_motion_estimation_info(short blk_x, 
                                                   short blk_y,
                                                   short x_motion_estimation,
                                                   short y_motion_estimation)
{
  /* Macroblock size 16X16, Block size is 4. (see lencod/inc/defines.h)
     Block_pos * 4 = real_pos */
  short x = blk_x * BLOCK_SIZE;
  short y = blk_y * BLOCK_SIZE;

  /* first see if we are tracking an object */
  if (!tracked_obj) {
    return;
  }

  
  /* Lets test if this block is inside our object area.  */
  if (!metadata_extractor_tracked_obj_block_is_inside(x, y, tracked_obj)) {
    /* Ignore this block info */
    return;
  }

  /* Since we are going to accumulate one vector to the entire object, just sum the estimation */
  tracked_obj->motion_x += x_motion_estimation;
  tracked_obj->motion_y += y_motion_estimation;
  tracked_obj->motion_samples++;
}
