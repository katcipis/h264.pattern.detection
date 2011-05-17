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

typedef struct _MetadataExtractor MetadataExtractor;


/*!
 *********************************************************************************
 * Creates a new metadata extractor.  
 *
 * @param min_width             Min width of the object that will be detected.
 * @param min_height            Min height of the object that will be detected.
 * @param search_hysteresis     Search for new object hysteresys (in frames).
 * @param tracking_hysteresis   Confirm tracked object existence hysteresis (in frames).
 * @param training_file         File containing the training info used on the object detection.
 *
 * @return A MetadataExtractor object.
 *
 *
 *********************************************************************************
 */
MetadataExtractor * metadata_extractor_new(int min_width,
                                           int min_height,
                                           int search_hysteresis,
                                           int tracking_hysteresis,
                                           const char * training_file);


/*!
 *********************************************************************************
 * Free a metadata extractor.  
 *
 * @param extractor The metadata extractor object to be destroyed.
 *
 *
 *********************************************************************************
 */
void metadata_extractor_free(MetadataExtractor * extractor);


/*!
 *********************************************************************************
 * Extracts the interest object bounding box as a metadata from the Y plane.
 *
 * @param extractor    The MetadataExtractor object.
 * @param frame_number The frame number.
 * @param y            The luma plane, y[i][j] where i is the row and j the column.
 * @param width        The luma plane width.
 * @param height       The luma plane height.
 *
 * @return The metadata or NULL if the interest object is not foundd on the frame.
 *
 *********************************************************************************
 */
ExtractedMetadata * metadata_extractor_extract_object_bounding_box(MetadataExtractor * extractor,
                                                                   unsigned int frame_number,
                                                                   unsigned char ** y,
                                                                   int width,
                                                                   int height);

/*!
 *********************************************************************************
 * Add usefull info about block motion estimation, allowing the extractor 
 * to do object tracking.Eliminating the need to process every frame 
 * to know the new position of a previously detected object.
 *
 * @param extractor           The MetadataExtractor object.
 * @param block_x             The block x position.
 * @param block_y             The block y position.
 * @param x_motion_estimation The motion estimation for x (in QPEL units). 
 * @param y_motion_estimation The motion estimation for y (in QPEL units).
 *
 *********************************************************************************
 */
void metadata_extractor_add_motion_estimation_info(MetadataExtractor * extractor,
                                                   short block_x, 
                                                   short block_y,
                                                   double x_motion_estimation,
                                                   double y_motion_estimation);

#endif
