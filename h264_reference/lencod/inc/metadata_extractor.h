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

/* Since JM reference software does a lot of nice good beautifull typedefs like uint64, int64, etc.
   i cant use anything from the JM reference software here, it clashes with other beautifull typedefs made by 
   OpenCV (including global.h breaks everything). Isnt just great when people do typedefs with GOOD names ? :-)
   yeah, long names seems to be nasty, but at least they dont CLASH with other type names ;-) */

typedef struct _ExtractedMetadata {
  char * data;
  unsigned int size;
} ExtractedMetadata;

/*!
 *****************************************************************************
 * Extracts metadata from a YUV 4:2:0 planar image.
 *
 * @param y The luma plane.
 * @param u One chroma plane.
 * @param v One chroma plane.
 * @param width The luma plane width.
 * @param height The luma plane height.
 * @param image_type Type of the YUV image.
 *
 * @return A Metadata object containing metadata information, or NULL if no 
 *         usefull metadata is found on the image.
 *
 *****************************************************************************
 */
ExtractedMetadata * metadata_extractor_extract_from_yuv_420(unsigned char * y, 
                                                            unsigned char * u, 
                                                            unsigned char * v,
                                                            int width, 
                                                            int height);

#endif
