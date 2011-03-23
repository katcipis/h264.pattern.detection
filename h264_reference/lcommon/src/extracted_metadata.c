#include "extracted_metadata.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/* types/struct definition */
typedef void (*ExtractedMetadataFreeFunc) (ExtractedMetadata *);
typedef void (*ExtractedMetadataSerializeFunc) (ExtractedMetadata *, char *);
typedef int  (*ExtractedMetadataGetSerializedSizeFunc) (ExtractedMetadata *);

typedef enum {
  /* 1 byte only */
  ExtractedMetadataYUVImage = 0x01
} ExtractedMetadataType;

struct _ExtractedMetadata {
  ExtractedMetadataType type;
  ExtractedMetadataFreeFunc free;
  ExtractedMetadataSerializeFunc serialize;
  ExtractedMetadataGetSerializedSizeFunc get_serialized_size;
};

struct _ExtractedYUVImage {
  ExtractedMetadata parent;
  /* Image plane - 8 bits depth */
  unsigned char ** y;
  /* Image size*/
  uint16_t width;
  uint16_t height;
};

static const int EXTRACTED_METADATA_TYPE_SIZE = 1;

/*
 ********************************
 * ExtractedMetadata Public API *
 ********************************
 */
void extracted_metadata_free(ExtractedMetadata * metadata)
{
  metadata->free(metadata);
}

int extracted_metadata_get_serialized_size(ExtractedMetadata * metadata)
{
  return metadata->get_serialized_size(metadata) + EXTRACTED_METADATA_TYPE_SIZE;
}

void extracted_metadata_serialize(ExtractedMetadata * metadata, char * serialized_data)
{
  /* lets write the media type (1 byte) */
  *serialized_data = (char) metadata->type;
  metadata->serialize(metadata, serialized_data + EXTRACTED_METADATA_TYPE_SIZE);
}

ExtractedMetadata * extracted_metadata_deserialize(const char * data, int size)
{
  return NULL;
}

/*
 *********************************
 * ExtractedMetadata Private API *
 *********************************
 */
static void extracted_metadata_init(ExtractedMetadata * metadata, 
                                    ExtractedMetadataFreeFunc free, 
                                    ExtractedMetadataSerializeFunc serialize,
                                    ExtractedMetadataGetSerializedSizeFunc get_serialized_size,
                                    ExtractedMetadataType type)
{
  metadata->free                = free;
  metadata->serialize           = serialize;
  metadata->get_serialized_size = get_serialized_size;
  metadata->type                = type;
}


/*
 ********************************
 * ExtractedYUVImage Public API *
 ********************************
 */

static void extracted_yuv_image_free(ExtractedMetadata * metadata);
static void extracted_yuv_image_serialize (ExtractedMetadata * metadata, char * data);
static int extracted_yuv_image_get_serialized_size(ExtractedMetadata * metadata);

ExtractedYUVImage * extracted_yuv_image_new(int width, int height)
{
  ExtractedYUVImage * metadata = malloc(sizeof(ExtractedYUVImage));
  unsigned char ** y_rows   = malloc(sizeof(unsigned char *) * height);
  unsigned char * y_plane   = malloc(sizeof(unsigned char) * height * width);
  int row_offset            = 0;
  int row;

  /* Filling the rows */
  for (row = 0; row < height; row++) {

    y_rows[row] = y_plane + row_offset;
    row_offset += width;
  }

  metadata->height      = height;
  metadata->width       = width;
  metadata->y           = y_rows;
  
  extracted_metadata_init((ExtractedMetadata *) metadata,
                          extracted_yuv_image_free,
                          extracted_yuv_image_serialize,
                          extracted_yuv_image_get_serialized_size,
                          ExtractedMetadataYUVImage);

  return metadata;
}

unsigned char ** extracted_yuv_image_get_y(ExtractedYUVImage * img)
{
  return img->y;
}


/*
 ************************************
 * ExtractedYUVImage private functions *
 ************************************
 */

static void extracted_yuv_image_free(ExtractedMetadata * metadata)
{
  /* metadata->y[0] points to the begin of the y plane. Easy to free all data */
  ExtractedYUVImage * img = (ExtractedYUVImage *) metadata;

  /* freeing the image plane */
  free(img->y[0]);

  /* freeing the rows array */
  free(img->y);
}

static void extracted_yuv_image_serialize (ExtractedMetadata * metadata, char * data)
{
  ExtractedYUVImage * img = (ExtractedYUVImage *) metadata;

  /* avoid problems with type size and endianness */
  *((uint16_t *) data) = htons(img->width);
  data += sizeof(uint16_t);

  *((uint16_t *) data) = htons(img->height);
  data += sizeof(uint16_t);

  /* Lets copy all the plane. Pretty easy since y[0] points to the begin of the plane */
  memcpy(data, img->y[0], img->width * img->height * sizeof(unsigned char));
}

static int extracted_yuv_image_get_serialized_size(ExtractedMetadata * metadata)
{
  ExtractedYUVImage * img = (ExtractedYUVImage *) metadata;
  return sizeof(uint16_t) + sizeof(uint16_t) + img->width * img->height;
}

