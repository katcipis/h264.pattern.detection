#include "metadata_extractor.h"
#include <cv.h>
#include <cvaux.h>
#include <highgui.h>


static IplImage* metadata_extractor_from_planar_yuv_to_interleaved_yuv444 (unsigned char * y, 
                                                                           unsigned char * u, 
                                                                           unsigned char * v,
                                                                           int width, 
                                                                           int height,
                                                                           int chroma_width,
                                                                           int chroma_height)
{
  IplImage *py, *pu, *pv, *pu_big, *pv_big, *image;

  /* Lets create one different image to each YUV plane */
  py = cvCreateImage(cvSize(width, height),     IPL_DEPTH_8U, 1);
  pu = cvCreateImage(cvSize(chroma_width, chroma_height), IPL_DEPTH_8U, 1);
  pv = cvCreateImage(cvSize(chroma_width, chroma_height), IPL_DEPTH_8U, 1);

  pu_big = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  pv_big = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);

  image = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

  /* We assume that imgpel has the size of a byte, same as IPL_DEPTH_8U */

  /* Read Y */
  memcpy(py->imageData, y, width * height); 

  /* Read U */
  memcpy(pu->imageData, u, chroma_width * chroma_height);

  /* Read V */
  memcpy(pv->imageData, v, chroma_width * chroma_height);

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
ExtractedMetadata * metadata_extractor_extract_from_yuv_420(unsigned char * y, unsigned char * u, unsigned char * v,
                                                            int width, int height)
{
  /* OpenCV only works with interleaved BGR images (learning OpenCV p.43, footnote).
     Here we have planar YUV frames. Lets do some convertions. */

  CvArr* src = NULL;
  CvArr* dst = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

  /* OpenCV needs a 4:4:4 interleaved YUV image */
  src = metadata_extractor_from_planar_yuv_to_interleaved_yuv444(y, u, v, width, height, width / 4, height / 4);
  
  /* Allocate the destiny BGR image */
  cvCvtColor(src, dst, CV_YCrCb2BGR);

  cvNamedWindow("metadata_extractor_get_metadata_from_yuv_image", 1);
  cvShowImage("metadata_extractor_get_metadata_from_yuv_image", dst);
  cvWaitKey(1000);


  return NULL;
}

