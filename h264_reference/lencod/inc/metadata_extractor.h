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

#include "extracted_metadata.h"


/*!
 *****************************************************************************
 * Extracts metadata from the Y plane.
 *
 * @param y The luma plane, y[i][j] where i is the row and j the column.
 * @param width The luma plane width.
 * @param height The luma plane height.
 *
 * @return A NULL terminated array of Metadata objects containing metadata 
 *         information or NULL if no usefull metadata is found on the image.
 *
 *****************************************************************************
 */
ExtractedMetadata ** metadata_extractor_extract_from_y(unsigned char ** y, 
                                                       int width, 
                                                       int height);

#endif
