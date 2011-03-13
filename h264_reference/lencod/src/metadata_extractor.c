#include "metadata_extractor.h"
#include <cv.h>
#include <cvaux.h>
#include <highgui.h>

/* OpenCV redefines some types that already exists on JM reference software. */
#undef int64
#undef uint64


static IplImage* metadata_extractor_from_planar_yuv420_to_interleaved_yuv444(ImageData *img)
{
  IplImage *py, *pu, *pv, *pu_big, *pv_big, *image;
  int luma_width, chroma_width;
  int luma_height, chroma_height;

  luma_width    = img->format.width[0];
  luma_height   = img->format.height[0];
  chroma_width  = img->format.width[1];
  chroma_height = img->format.height[1];

  /* Lets create one different image to each YUV plane */
  py = cvCreateImage(cvSize(luma_width, luma_height),     IPL_DEPTH_8U, 1);
  pu = cvCreateImage(cvSize(chroma_width, chroma_height), IPL_DEPTH_8U, 1);
  pv = cvCreateImage(cvSize(chroma_width, chroma_height), IPL_DEPTH_8U, 1);

  pu_big = cvCreateImage(cvSize(luma_width, luma_height), IPL_DEPTH_8U, 1);
  pv_big = cvCreateImage(cvSize(luma_width, luma_height), IPL_DEPTH_8U, 1);

  image = cvCreateImage(cvSize(luma_width, luma_height), IPL_DEPTH_8U, 3);

  /* We assume that imgpel has the size of a byte, same as IPL_DEPTH_8U */

  /* Read Y */
  memcpy(py->imageData, img->frm_data[0][0], luma_width * luma_height * sizeof (imgpel)); 

  /* Read U */
  memcpy(pu->imageData, img->frm_data[1][0], chroma_width * chroma_height * sizeof (imgpel));

  /* Read V */
  memcpy(pv->imageData, img->frm_data[2][0], chroma_width * chroma_height * sizeof (imgpel));

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

static IplImage* metadata_extractor_from_planar_yuv444_to_interleaved_yuv444(ImageData *img)
{
  /* TODO */
  return NULL;
}

/*!
 *************************************************************************************
 * \brief
 *    Function body for extract metadata from a yuv image.
 *
 * \return
 *    The Metadata object or NULL.
 *
 *************************************************************************************
 */
Metadata * metadata_extractor_get_metadata_from_yuv_image(ImageData *img)
{
  /* OpenCV only works with interleaved BGR images (learning OpenCV p.43, footnote).
     Here we have planar YUV frames. Lets do some convertions. */

  CvArr* src = NULL;
  CvArr* dst = cvCreateImage(cvSize(img->format.width[0], img->format.height[0]), IPL_DEPTH_8U, 3);

  /* OpenCV needs a 4:4:4 interleaved YUV image */
  switch(img->format.yuv_format) 
  {
    case YUV400:
      /* TODO */
      return NULL;
      break;

    case YUV422:
      /* TODO */
      return NULL;
      break;

    case YUV420:
      /* the 2 chroma are then upsampled by a 2 factor, and return an image composed by 3 layer (YUV 4:4:4) */
      src = metadata_extractor_from_planar_yuv420_to_interleaved_yuv444(img);
      break;

    case YUV444:
      /* Work already done, just interleave the image */
      src = metadata_extractor_from_planar_yuv444_to_interleaved_yuv444(img);
      break;

    default:
      /* Panic ? */
      return NULL;
      break;
  }  
  
  /* Allocate the destiny BGR image */
  cvCvtColor(src, dst, CV_YCrCb2BGR);

  cvNamedWindow("metadata_extractor_get_metadata_from_yuv_image", 1);
  cvShowImage("metadata_extractor_get_metadata_from_yuv_image", dst);
  cvWaitKey(1000);

  /* Do more stuff :-) */
  printf("Frame width0[%d] width1[%d] width2[%d]\n", img->format.width[0], img->format.width[1], img->format.width[2]);
  printf("Frame height0[%d] height1[%d] height2[%d]\n", img->format.height[0], img->format.height[1], img->format.height[2]);

  return NULL;
}
