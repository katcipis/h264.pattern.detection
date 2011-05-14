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
 *********************************************************************************
 * Extracts the entire raw interest object as a metadata from the Y plane.
 *
 * @param frame_number The frame number.
 * @param y The luma plane, y[i][j] where i is the row and j the column.
 * @param width The luma plane width.
 * @param height The luma plane height.
 *
 * @return The metadata or NULL if the interest object is not foundd on the frame.
 *
 *********************************************************************************
 */
ExtractedMetadata * metadata_extractor_extract_raw_object(unsigned int frame_number,
                                                          unsigned char ** y, 
                                                          int width, 
                                                          int height);

/*!
 *********************************************************************************
 * Extracts the interest object bounding box as a metadata from the Y plane.
 *
 * @param frame_number The frame number.
 * @param y The luma plane, y[i][j] where i is the row and j the column.
 * @param width The luma plane width.
 * @param height The luma plane height.
 *
 * @return The metadata or NULL if the interest object is not foundd on the frame.
 *
 *********************************************************************************
 */
ExtractedMetadata * metadata_extractor_extract_object_bounding_box(unsigned int frame_number,
                                                                   unsigned char ** y,
                                                                   int width,
                                                                   int height);

/*!
 *********************************************************************************
 * Add usefull info about block motion estimation, 
 * allowing the extractor to do object tracking.
 *
 * @param block_x The block x position.
 * @param block_y The block y position.
 * @param x_motion_estimation The motion estimation for x. 
 * @param y_motion_estimation The motion estimation for y.
 *
 *********************************************************************************
 */
void metadata_extractor_add_motion_estimation_info(short block_x, 
                                                   short block_y,
                                                   short x_motion_estimation,
                                                   short y_motion_estimation);

#endif
