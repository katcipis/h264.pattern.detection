/*!
 *******************************************************************************
 *  \file
 *     metadata_extractor.h
 *  \brief
 *     definitions for the metadata extractor.
 *  \author(s)
 *      - Tiago Katcipis                             <tiagokatcipis@gmail.com>
 *
 * *****************************************************************************
 */

#ifndef METADATA_EXTRACTOR_H
#define METADATA_EXTRACTOR_H

#include "global.h"

typedef struct _Metadata {
    char * data;
    unsigned int size;
} Metadata;

/*!
 *****************************************************************************
 * Extracts metadata from a raw YUV image.
 *
 * @param img The raw YUV image.
 * @return A Metadata object containing metadata information, or NULL if no 
 *         usefull metadata is found on the image
 *
 *****************************************************************************
 */
Metadata * metadata_extractor_get_metadata_from_yuv_image(ImageData *img);

#endif
