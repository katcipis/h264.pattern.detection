#include "metadata_extractor.h"
#include <cv.h>
#include <cvaux.h>
#include <highgui.h>
#include <stdio.h>

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
ExtractedMetadata * metadata_extractor_extract_from_yuv(unsigned char ** y, int width, int height)
{
  /* First we must convert the Y luma plane to BGR Grayscale. 
     On grayscale Y = R = G = B. Pretty simple to convert. */

  IplImage * frame = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
  int frame_i = 0;
  int row = 0;
  int col = 0;

  /* We must write R G B with the same Y sample*/
  for (row = 0; row < height; row++) {

    for (col = 0; col < width; col++) {

      frame->imageData[frame_i] = y[row][col];
      frame->imageData[frame_i + 1] = y[row][col];
      frame->imageData[frame_i + 2] = y[row][col];
      frame_i += 3;
    }
  }

  cvNamedWindow("frame", 1);
  cvShowImage("frame", frame);

  cvWaitKey(1000);

  return NULL;
}

