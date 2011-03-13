#include "metadata_extractor.h"

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
    printf("Frame width0[%d] width1[%d] width2[%d]\n", img->format.width[0], img->format.width[1], img->format.width[2]);
    printf("Frame height0[%d] height1[%d] height2[%d]\n", img->format.height[0], img->format.height[1], img->format.height[2]);

    return NULL;
}
