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

typedef enum {
  MetadataExtractor_YUV400,
  MetadataExtractor_YUV420,
  MetadataExtractor_YUV422,
  MetadataExtractor_YUV444
} MetadataExtractorYUVImageType;

/* ImageData will be opaque here in*/

/*!
 *****************************************************************************
 * Extracts metadata from a YUV planar image.
 * The chroma size can be guessed using the image type.
 *
 * @param y The luma plane.
 * @param u The chroma plane.
 * @param v The chroma plane.
 * @param width The luma plane width.
 * @param height The luma plane height.
 * @param image_type Type of the YUV image.
 *
 * @return A Metadata object containing metadata information, or NULL if no 
 *         usefull metadata is found on the image.
 *
 *****************************************************************************
 */
ExtractedMetadata * metadata_extractor_extract(unsigned char * y, unsigned char * u, unsigned char * v,
                                               int width, int height, MetadataExtractorYUVImageType image_type);

#endif
