#include "extracted_metadata.h"
#include <stdlib.h>

typedef void (*ExtractedMetadataFreeFunc) (ExtractedMetadata *);

struct _ExtractedMetadata {
  ExtractedMetadataFreeFunc free;
};

struct _ExtractedYUVImage {
  ExtractedMetadata parent;
  unsigned char ** y;
  int width;
  int height;
};

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
  return 0;
}

void extracted_metadata_serialize(ExtractedMetadata * metadata, char * serialized_data)
{

}

ExtractedMetadata * extracted_metadata_deserialize(const char * data, int size)
{
  return NULL;
}

/*
 *****************************
 * ExtractedYUVImage Public API *
 *****************************
 */

static void extracted_yuv_image_free(ExtractedMetadata * metadata);

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
  metadata->parent.free = extracted_yuv_image_free;

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
