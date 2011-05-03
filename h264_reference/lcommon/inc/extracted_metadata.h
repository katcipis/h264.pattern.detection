/*!
 *******************************************************************************
 *  \file
 *     extracted_metadata.h
 *  \brief
 *     definitions for the extracted metadata.
 *  \author(s)
 *      - Tiago Katcipis                             <tiagokatcipis@gmail.com>
 *
 * *****************************************************************************
 */

#ifndef EXTRACTED_METADATA_H
#define EXTRACTED_METADATA_H

typedef struct _ExtractedYImage ExtractedYImage;
typedef struct _ExtractedMetadata ExtractedMetadata;
typedef struct _ExtractedMetadataBuffer ExtractedMetadataBuffer;
typedef struct _ExtractedObjectBoundingBox ExtractedObjectBoundingBox;


/* ExtractedObjectBoundingBox API */

/*!
 *******************************************************************************
 * Creates a new extracted image, the image only has the luma plane (Y).
 *
 * @param frame_num The frame number this metadata belongs.
 * @param x The x coordinate of the bounding box.
 * @param y The y coordinate of the bounding box.
 * @param width The width of the bounding box.
 * @param height The height of the bounding box.
 * @return The newly allocated ExtractedObjectBoundingBox object.
 *
 *******************************************************************************
 */
ExtractedObjectBoundingBox * extracted_object_bounding_box_new(unsigned int frame_num, int x, int y, int width, int height);


/* ExtractedYImage API */

/*!
 *******************************************************************************
 * Creates a new extracted image, the image only has the luma plane (Y).
 *
 * @param frame_num The frame number this metadata belongs.
 * @param width The width of the image that will be saved.
 * @param height The height of the image that will be saved.
 * @return The newly allocated ExtractedImage object.
 *
 *******************************************************************************
 */
ExtractedYImage * extracted_y_image_new(unsigned int frame_num, int width, int height);

/*!
 *******************************************************************************
 * Get the luma plane of the extracted image.
 * y[row][col] format. Each pixel has 8 bits depth.
 *
 * @return The luma plane.
 *
 *******************************************************************************
 */
unsigned char ** extracted_y_image_get_y(ExtractedYImage * img);


/* ExtractedMetadata API */

/*!
 *******************************************************************************
 * Gets the size in bytes necessary to serialize this ExtractedMetadata object.
 *
 * @param metadata The metadata to get the serialized size in bytes.
 * @return The serialized size (in bytes) of this ExtractedMetadata.
 *
 *******************************************************************************
 */
int extracted_metadata_get_serialized_size(ExtractedMetadata * metadata);

/*!
 *******************************************************************************
 * Serialize this ExtractedMetadata object. It is responsability of the caller
 * to alloc a data pointer with the right size and to free it after usage.
 * The right size can be get with extracted_metadata_get_serialized_size.
 *
 * @param metadata The metadata to serialize.
 * @param serialized_data Where the serialized metadata will be stored.
 *
 *******************************************************************************
 */
void extracted_metadata_serialize(ExtractedMetadata * metadata, char * serialized_data);

/*!
 *******************************************************************************
 * Deserialize this ExtractedMetadata object. 
 *
 * @param data The serialized metadata.
 * @param size Size in bytes of the serialized object.
 * @return The deserialized ExtractedMetadata object or NULL in case of error.
 *
 *******************************************************************************
 */
ExtractedMetadata * extracted_metadata_deserialize(const char * data, int size);

/*!
 *******************************************************************************
 * Save this ExtractedMetadata object on a file. 
 *
 * @param metadata The metadata to be saved.
 * @param fd File descriptor where the metadata will be saved.
 *
 *******************************************************************************
 */
void extracted_metadata_save(ExtractedMetadata * metadata, int fd);

/*!
 *******************************************************************************
 * Frees all resources being used by a ExtractedMetadata object.
 *
 * @param metadata The metadata that will be freed.
 *
 *******************************************************************************
 */
void extracted_metadata_free(ExtractedMetadata * metadata);


/* ExtractedMetadataBuffer API */

/*!
 *******************************************************************************
 * Creates a new ExtractedMetadataBuffer object. 
 *
 * @return The metadata buffer object, or NULL in case of error.
 *
 *******************************************************************************
 */
ExtractedMetadataBuffer * extracted_metadata_buffer_new();

/*!
 *******************************************************************************
 * Add a metadata do the buffer. 
 *
 * @param buffer The metadata buffer object.
 * @param metadata The metadata object.
 *
 *******************************************************************************
 */
void extracted_metadata_buffer_add(ExtractedMetadataBuffer * buffer, ExtractedMetadata * obj);

/*!
 *******************************************************************************
 * Get a metadata from the buffer for the given frame, all late metadata will
 * be freed, if there is no metadata for this frame return NULL.
 *
 * @return The metadata object, or NULL if there is no metadata for this frame.
 *
 *******************************************************************************
 */
ExtractedMetadata * extracted_metadata_buffer_get(ExtractedMetadataBuffer * buffer, int frame_number);

#endif
