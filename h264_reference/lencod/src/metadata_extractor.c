#include "metadata_extractor.h"

#include <cv.h>
#include <highgui.h>
#include <stdio.h>


/*********************** 
 * Module private data * 
 ***********************/


typedef struct _TrackedBoundigBox {
  unsigned int id;
  
  /* double x,y is used so we dont lose small movement information */
  double x; 
  double y;
  short width;
  short height;

  int motion_x;
  int motion_y;
  int motion_samples;
} TrackedBoundigBox;


struct _MetadataExtractor {

  /* Haar feature cascade */
  CvHaarClassifierCascade * classifier;

  /* OpenCV “work buffer" */
  CvMemStorage * storage;

  /* How big of a jump there is between each scale */
  double scale_factor;

  /* only decide a face is present in a location if there are at least three overlapping detections. */
  int min_neighbors;

  /* Minimum object size */
  CvSize min_size;
  /* Maximum object size */
  CvSize max_size;

  int haar_flags;

  /* Search/Tracking hysteresis */
  unsigned int search_hysteresis;
  unsigned int tracking_hysteresis;
  
  /* Last frame that we did a full search */
  unsigned int last_searched_frame;

  TrackedBoundigBox * tracked_bounding_box;
};


static const double DEFAULT_SCALE_FACTOR = 1.1f;

static const int DEFAULT_MIN_NEIGHBORS   = 3;


/* The motion vectors are on QPEL units (Quarter Pel refinement). To get the block real movement we must divide by 4 */
static const double QPEL_UNIT = 4.0f;


/* Private TrackedBoundigBox functions */
static TrackedBoundigBox * tracked_bounding_box_new(CvRect* area);
static void tracked_bounding_box_update(TrackedBoundigBox * obj, CvRect* area);
static void tracked_bounding_box_free(TrackedBoundigBox * obj);
static void tracked_bounding_box_estimate_motion(TrackedBoundigBox * obj);
static int tracked_bounding_box_point_is_inside(short x, short y, TrackedBoundigBox * obj);

/*!
 *************************************************************************************
 * \brief
 *    Function body for the metadata extractor new.
 *
 *
 *************************************************************************************
 */
MetadataExtractor * metadata_extractor_new(int min_width,
                                           int min_height,
                                           int search_hysteresis,
                                           int tracking_hysteresis,
                                           const char * training_file)
{
  /* FIXME A new opencv added this is information, i dont have time to configure/parametrize it, sorry :-( */
  static const int max_detected_object_width  = 640;
  static const int max_detected_object_height = 640;

  MetadataExtractor * extractor = malloc(sizeof(MetadataExtractor));

  if (!extractor) {
    printf("metadata_extractor_new: Error allocating MetadataExtractor !!!\n");
    return NULL;
  }

  extractor->classifier            = (CvHaarClassifierCascade*) cvLoad( training_file, 0, 0, 0 );
  extractor->storage               = cvCreateMemStorage(0);
  extractor->min_size.width        = min_width; 
  extractor->min_size.height       = min_height;
  extractor->max_size.width        = max_detected_object_width; 
  extractor->max_size.height       = max_detected_object_height;
  extractor->search_hysteresis     = search_hysteresis;
  extractor->tracking_hysteresis   = tracking_hysteresis;

  /* default hardcoded stuff */
  extractor->haar_flags           = CV_HAAR_FIND_BIGGEST_OBJECT | CV_HAAR_DO_ROUGH_SEARCH | CV_HAAR_DO_CANNY_PRUNING;
  extractor->min_neighbors        = DEFAULT_MIN_NEIGHBORS;
  extractor->scale_factor         = DEFAULT_SCALE_FACTOR;
  extractor->tracked_bounding_box = NULL;
  extractor->last_searched_frame  = 0;

  printf("\nmetadata_extractor_new: configured to search for objects with a min size of [%d] x [%d]\n", 
         min_width, min_height);

  printf("metadata_extractor_new: search_hysteresis[%d] tracking_hysteresis[%d] training file is [%s]\n\n", 
         search_hysteresis, tracking_hysteresis, training_file);

  return extractor;
}

void metadata_extractor_free(MetadataExtractor * extractor)
{
  if (extractor->tracked_bounding_box) {
    tracked_bounding_box_free(extractor->tracked_bounding_box);
  }
  free(extractor);
}

static CvRect* metadata_extractor_search_for_object_of_interest(MetadataExtractor * extractor, 
                                                                unsigned char ** y, 
                                                                int width, 
                                                                int height)
{
  IplImage * frame = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
  IplImage * gray  = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  CvSeq* results   = NULL;

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

  cvCvtColor(frame, gray, CV_BGR2GRAY);

  /* cvEqualizeHist spreads out the brightness values necessary because the integral image 
     features are based on differences of rectangle regions and, if the histogram is not balanced, 
     these differences might be skewed by overall lighting or exposure of the test images. */
  cvEqualizeHist(gray, gray);

  /* prepares memory for the haar_classifier (if it was used before already) */
  cvClearMemStorage(extractor->storage);

  /* The search is optimized to find only one object */
  results =  cvHaarDetectObjects (gray,
                                  extractor->classifier,
                                  extractor->storage,
                                  extractor->scale_factor,
                                  extractor->min_neighbors,
                                  extractor->haar_flags, 
                                  extractor->min_size,
                                  extractor->max_size);

  /* Freeing images */
  cvReleaseImage(&frame);
  cvReleaseImage(&gray);

  if (results && (results->total <= 0) ) {
      return NULL;
  }

  return (CvRect*)cvGetSeqElem( results, 0);
}


/******************************** 
 * TrackedBoundigBox facilities * 
 ********************************/

/* Private const data */
static const int BLOCK_SIZE = 4;


/* Private TrackedBoundigBox functions */
static TrackedBoundigBox * tracked_bounding_box_new(CvRect* area)
{
  static unsigned int tracked_bounding_box_id = 0;

  TrackedBoundigBox * obj = malloc(sizeof(TrackedBoundigBox));

  if (!obj) {
    return NULL;
  }

  obj->id = tracked_bounding_box_id;
  tracked_bounding_box_update(obj, area);

  tracked_bounding_box_id++;
  return obj;
}

static void tracked_bounding_box_update(TrackedBoundigBox * box, CvRect* area)
{
  box->x              = area->x;
  box->y              = area->y;
  box->width          = area->width;
  box->height         = area->height;
  box->motion_x       = 0;
  box->motion_y       = 0;
  box->motion_samples = 0;
}

static void tracked_bounding_box_free(TrackedBoundigBox * obj)
{
  free(obj);
}

static void tracked_bounding_box_estimate_motion(TrackedBoundigBox * obj) 
{
  /* A simple arithmetic mean of all the vectors */

  /* Dont want to lose precision (accumulated movement) on the integer division */
  obj->x -= ((double) obj->motion_x / QPEL_UNIT) / (double) obj->motion_samples;
  obj->y -= ((double) obj->motion_y / QPEL_UNIT) / (double) obj->motion_samples;

  obj->motion_x       = 0;
  obj->motion_y       = 0;
  obj->motion_samples = 0;
}

static int tracked_bounding_box_point_is_inside(short x, short y, TrackedBoundigBox * obj)
{

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

/*************
* Public API *
**************/
ExtractedMetadata * metadata_extractor_extract_object_bounding_box(MetadataExtractor * extractor, 
                                                                   unsigned int frame_num, 
                                                                   unsigned char ** y,
                                                                   int width,
                                                                   int height)
{
  CvRect * rect = NULL;

  if (extractor->tracked_bounding_box) {

    /* We are tracking - lets check our tracking hysteresis and if we have 
       some motion samples to work with. (means the object left the video area) */
    if ( ((frame_num - extractor->last_searched_frame) >= extractor->tracking_hysteresis) ||
         (extractor->tracked_bounding_box->motion_samples == 0) ) {

      /* Time to confirm if the object is still present */
      rect = metadata_extractor_search_for_object_of_interest(extractor, y, width, height);
      extractor->last_searched_frame = frame_num;

      if (!rect) {
        /* We are tracking something that no longer exists. */
        tracked_bounding_box_free(extractor->tracked_bounding_box);
        extractor->tracked_bounding_box = NULL;
        return NULL;
      }

      /* We still have the object. It would be good to have some heuristic to verify if it is the same object */
      tracked_bounding_box_update(extractor->tracked_bounding_box, rect);

    } else {

      /* Just use ME information  */
      tracked_bounding_box_estimate_motion(extractor->tracked_bounding_box);

    }

    return (ExtractedMetadata *) extracted_object_bounding_box_new(extractor->tracked_bounding_box->id, 
                                                                   frame_num, 
                                                                   extractor->tracked_bounding_box->x, 
                                                                   extractor->tracked_bounding_box->y, 
                                                                   extractor->tracked_bounding_box->width, 
                                                                   extractor->tracked_bounding_box->height);
  }

  /* We are not tracking */

  if ( (frame_num - extractor->last_searched_frame) < extractor->search_hysteresis) {
    /* Lets not search this frame */
    return NULL;
  }

  rect = metadata_extractor_search_for_object_of_interest(extractor, y, width, height);
  extractor->last_searched_frame = frame_num;

  if (!rect) {
    /* no interest object */
    return NULL;
  }

  extractor->tracked_bounding_box = tracked_bounding_box_new(rect); 
  return (ExtractedMetadata *) extracted_object_bounding_box_new(extractor->tracked_bounding_box->id, 
                                                                 frame_num, 
                                                                 extractor->tracked_bounding_box->x, 
                                                                 extractor->tracked_bounding_box->y, 
                                                                 extractor->tracked_bounding_box->width, 
                                                                 extractor->tracked_bounding_box->height);
}

void metadata_extractor_add_motion_estimation_info(MetadataExtractor * extractor, 
                                                   short blk_x, 
                                                   short blk_y,
                                                   short x_motion_estimation,
                                                   short y_motion_estimation)
{
  /* Block size is 4x4. (see lencod/inc/defines.h)
     block_pos * BLOCK_SIZE = real_pos */
  short x = blk_x * BLOCK_SIZE;
  short y = blk_y * BLOCK_SIZE;

  /* first see if we are tracking an object */
  if (!extractor->tracked_bounding_box) {
    return;
  }

  
  /* Lets test if any point of this block is inside our object area.  */
  if (! (tracked_bounding_box_point_is_inside(x, y, extractor->tracked_bounding_box) ||
         tracked_bounding_box_point_is_inside(x + BLOCK_SIZE, y, extractor->tracked_bounding_box) ||
         tracked_bounding_box_point_is_inside(x, y + BLOCK_SIZE, extractor->tracked_bounding_box) ||
         tracked_bounding_box_point_is_inside(x + BLOCK_SIZE, y + BLOCK_SIZE, extractor->tracked_bounding_box))) {
    /* Ignore this block info */ 
    return;
  }

  /* Since we are going to accumulate one vector to the entire object, just sum the estimation */
  extractor->tracked_bounding_box->motion_x += x_motion_estimation;
  extractor->tracked_bounding_box->motion_y += y_motion_estimation;
  extractor->tracked_bounding_box->motion_samples++;
}

/*!
*************************************************************************************
* \brief
* Function body for extract metadata from the y image plane.
*
* \return
* A ExtractedMetadata object or NULL in case no metadata is extracted.
*
*************************************************************************************
*/
ExtractedMetadata * metadata_extractor_extract_raw_object(MetadataExtractor * extractor,
                                                          unsigned int frame_num,
                                                          unsigned char ** y,
                                                          int width,
                                                          int height)
{
  /* First we must convert the Y luma plane to BGR and them to grayscale.
On grayscale Y = R = G = B. Pretty simple to convert. */

  ExtractedYImage * metadata = NULL;
  CvRect* res = NULL;

  res = metadata_extractor_search_for_object_of_interest(extractor, y, width, height);

  if (!res) {
      return NULL;
  }

  metadata = extracted_y_image_new(frame_num, res->width, res->height);

  unsigned char ** y_plane = extracted_y_image_get_y(metadata);
  int metadata_row = 0;
  int row = 0;
  int col = 0;

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

