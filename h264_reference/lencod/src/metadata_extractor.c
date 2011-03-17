#include "metadata_extractor.h"
#include <cv.h>
#include <cvaux.h>
#include <highgui.h>

/*
* Given a yuv subsampled planar image we extract the luma and chroma components,
* the 2 chroma are then upsampled by a 2 factor, and return an image composed by 3
* layer Y, U and V (format YUV 444, i.e. 3 byte for each pixel)
*/
static IplImage* metadata_extractor_from_planar_yuv_to_interleaved_yuv444 (unsigned char ** y, 
                                                                           unsigned char ** u, 
                                                                           unsigned char ** v,
                                                                           int width, 
                                                                           int height,
                                                                           int chroma_width,
                                                                           int chroma_height)
{
  /* http://tech.groups.yahoo.com/group/OpenCV/message/59027 */

  IplImage *py, *pu, *pv, *pu_big, *pv_big, *image;
  int i;
  char * tmp_y = NULL;
  char * tmp_u = NULL;
  char * tmp_v = NULL;

  /* Lets create one different image to each YUV plane */
  py = cvCreateImage(cvSize(width, height),     IPL_DEPTH_8U, 1);
  pu = cvCreateImage(cvSize(chroma_width, chroma_height), IPL_DEPTH_8U, 1);
  pv = cvCreateImage(cvSize(chroma_width, chroma_height), IPL_DEPTH_8U, 1);

  pu_big = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  pv_big = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);

  image = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

  /* We assume that imgpel (each pixel) has the size of a byte, same as IPL_DEPTH_8U */

  /* Read Y - row by row*/
  tmp_y = py->imageData;
  for (i = 0; i < height; i++) {
      /* copying line row y[i] to opencv image data */
      memcpy(tmp_y, y[i], width);
      tmp_y += width;
  }

  /* Read U and V */
  tmp_u = pu->imageData;
  tmp_v = pv->imageData;

  for (i = 0; i < chroma_height; i++) {
      /* copying line row u[i] to opencv image data */
      memcpy(tmp_u, u[i], chroma_width);
      /* copying line row u[i] to opencv image data */
      memcpy(tmp_v, v[i], chroma_width);
      tmp_u += chroma_width;
      tmp_v += chroma_width;
  }

  cvResize(pu, pu_big, CV_INTER_LINEAR);
  cvResize(pv, pv_big, CV_INTER_LINEAR);

  cvReleaseImage(&pu);
  cvReleaseImage(&pv);

  cvMerge(py, pu_big, pv_big, NULL, image);

  cvReleaseImage(&py);
  cvReleaseImage(&pu_big);
  cvReleaseImage(&pv_big);

  return image;
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
ExtractedMetadata * metadata_extractor_extract_from_yuv(unsigned char ** y, unsigned char ** u, unsigned char ** v,
                                                        int width, int height, int chroma_width, int chroma_height)
{
  /* OpenCV only works with interleaved BGR images (learning OpenCV p.43, footnote).
     Here we have planar YUV frames. Lets do some convertions. */

  CvArr* src = NULL;
  CvArr* dst = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

  /* OpenCV needs a 4:4:4 interleaved YUV image */
  src = metadata_extractor_from_planar_yuv_to_interleaved_yuv444(y, u, v, width, height, chroma_width, chroma_height);
  
  /* Allocate the destiny BGR image */
  cvCvtColor(src, dst, CV_YCrCb2BGR);

  cvNamedWindow("metadata_extractor_get_metadata_from_yuv_image", 1);
  cvShowImage("metadata_extractor_get_metadata_from_yuv_image", dst);
  cvWaitKey(1000);


  return NULL;
}

