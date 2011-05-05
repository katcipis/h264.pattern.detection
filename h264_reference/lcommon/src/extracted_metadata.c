#include "extracted_metadata.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>


/* types/struct definition */
typedef void (*ExtractedMetadataFreeFunc) (ExtractedMetadata *);
typedef void (*ExtractedMetadataSerializeFunc) (ExtractedMetadata *, char *);
typedef void (*ExtractedMetadataSaveFunc) (ExtractedMetadata *, int);
typedef int  (*ExtractedMetadataGetSerializedSizeFunc) (ExtractedMetadata *);

typedef enum {
  /* 1 byte only */
  ExtractedMetadataYImage            = 0x01,
  ExtractedMetadataObjectBoundingBox = 0x02
} ExtractedMetadataType;

struct _ExtractedMetadata {
  uint32_t frame_number;
  ExtractedMetadataType type;
  ExtractedMetadataFreeFunc free;
  ExtractedMetadataSerializeFunc serialize;
  ExtractedMetadataGetSerializedSizeFunc get_serialized_size;
  ExtractedMetadataSaveFunc save;
};

struct _ExtractedMetadataBuffer {
  ExtractedMetadata ** ringbuffer;
  short read_index;
  short write_index;
};

struct _ExtractedYImage {
  ExtractedMetadata parent;
  /* Image plane - 8 bits depth */
  unsigned char ** y;
  /* Image size*/
  uint16_t width;
  uint16_t height;
};

struct _ExtractedObjectBoundingBox {
  ExtractedMetadata parent;
  uint32_t id;
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

static const int EXTRACTED_METADATA_TYPE_SIZE = 1;

static ExtractedMetadata * extracted_y_image_deserialize(const char * data, int size);
static ExtractedMetadata * extracted_object_bounding_box_deserialize(const char * data, int size);

static int extracted_metadata_header_size()
{
  /* type size + frame number size */
  return EXTRACTED_METADATA_TYPE_SIZE + sizeof(uint32_t);
}

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
  /* Subclass size + metadata header size */
  return metadata->get_serialized_size(metadata) + extracted_metadata_header_size();
}

void extracted_metadata_serialize(ExtractedMetadata * metadata, char * serialized_data)
{
  /* lets write the media type (1 byte) */
  *serialized_data = (char) metadata->type;
  serialized_data += sizeof(char);

  /* lets write the frame number (4 bytes) */
  *((uint32_t *) serialized_data) = htonl(metadata->frame_number);
  serialized_data                += sizeof(uint32_t);

  printf("extracted_metadata_serialize: serialized frame_number[%u]\n", metadata->frame_number);
  metadata->serialize(metadata, serialized_data);
}

ExtractedMetadata * extracted_metadata_deserialize(const char * data, int size)
{
  ExtractedMetadataType type = 0;
  uint32_t frame_number      = 0;
  ExtractedMetadata * ret    = NULL;

  if (size < extracted_metadata_header_size()) {
    printf("extracted_metadata_deserialize: Mininum metadata size [%d], data size is [%d]\n", 
           size, extracted_metadata_header_size());
    return NULL;
  }

  /* first byte is the metadata type */
  type  = (ExtractedMetadataType) *data;
  data += EXTRACTED_METADATA_TYPE_SIZE;
  size -= EXTRACTED_METADATA_TYPE_SIZE;

  /* next 4 bytes are the frame number of the metadata */
  frame_number = ntohl(*((uint32_t *) data));

  data        += sizeof(uint32_t);
  size        -= sizeof(uint32_t);

  switch (type) 
  {
    case ExtractedMetadataYImage:
      ret = extracted_y_image_deserialize(data, size);
      break;

    case ExtractedMetadataObjectBoundingBox:
      ret = extracted_object_bounding_box_deserialize(data, size);
      break;
    default:
      printf("extracted_metadata_deserialize: cant find the extracted metadata type !!!\n");
  }
 
  if (ret) {
    ret->frame_number = frame_number;
  }

  return ret;
}

void extracted_metadata_save(ExtractedMetadata * metadata, int fd)
{
  metadata->save(metadata, fd);
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
                                    ExtractedMetadataSaveFunc save,
                                    ExtractedMetadataType type,
                                    uint32_t frame_number)
{
  metadata->free                = free;
  metadata->serialize           = serialize;
  metadata->get_serialized_size = get_serialized_size;
  metadata->save                = save;
  metadata->type                = type;
  metadata->frame_number        = frame_number;
}

/*
 *****************************************
 * ExtractedObjectBoundingBox Public API *
 *****************************************
 */

static void extracted_object_bounding_box_free(ExtractedMetadata * metadata);
static void extracted_object_bounding_box_serialize (ExtractedMetadata * metadata, char * data);
static int extracted_object_bounding_box_get_serialized_size(ExtractedMetadata * metadata);
static void extracted_object_bounding_box_save(ExtractedMetadata * metadata, int fd);

ExtractedObjectBoundingBox * extracted_object_bounding_box_new(unsigned int frame_num, int x, int y, int width, int height)
{
    static uint32_t bounding_box_id           = 0;
    ExtractedObjectBoundingBox * bounding_box = malloc(sizeof(ExtractedObjectBoundingBox));

    bounding_box->id         = bounding_box_id;
    bounding_box->x = x;
    bounding_box->y = y;
    bounding_box->width      = width;
    bounding_box->height     = height;

    extracted_metadata_init((ExtractedMetadata *) bounding_box,
                             extracted_object_bounding_box_free,
                             extracted_object_bounding_box_serialize,
                             extracted_object_bounding_box_get_serialized_size,
                             extracted_object_bounding_box_save,
                             ExtractedMetadataObjectBoundingBox,
                             frame_num);

    bounding_box_id++;
    return bounding_box;
}


/*
 ************************************
 * ExtractedYImage private functions *
 ************************************
 */

static void extracted_object_bounding_box_free(ExtractedMetadata * metadata)
{
    free(metadata);
}

static void extracted_object_bounding_box_serialize (ExtractedMetadata * metadata, char * data)
{
    ExtractedObjectBoundingBox * bounding_box = (ExtractedObjectBoundingBox *) metadata;
   
    *((uint32_t *) data) = htons(bounding_box->id);
    data += sizeof(uint32_t);

    *((uint16_t *) data) = htons(bounding_box->x);
    data += sizeof(uint16_t);

    *((uint16_t *) data) = htons(bounding_box->y);
    data += sizeof(uint16_t);

    *((uint16_t *) data) = htons(bounding_box->width);
    data += sizeof(uint16_t);

    *((uint16_t *) data) = htons(bounding_box->height);
    data += sizeof(uint16_t);
}

static ExtractedMetadata * extracted_object_bounding_box_deserialize(const char * data, int size)
{
  
  uint16_t width;
  uint16_t height;
  uint16_t x;
  uint16_t y;
  uint32_t id;

  /* Dont need the actual object to know the serialized size (fixed size). Not true for all objects */
  if (size < extracted_object_bounding_box_get_serialized_size(NULL)) {
    printf("extracted_object_bounding_box_deserialize: invalid serialized ExtractedObjectBoundingBox !!!\n");
    return NULL;
  }

  /* avoid problems with type size and endianness */
  id = ntohl(*((uint32_t *) data));
  data += sizeof(uint32_t);

  x = ntohs(((uint16_t *) data)[0]);
  y = ntohs(((uint16_t *) data)[1]);
  width = ntohs(((uint16_t *) data)[2]);
  height = ntohs(((uint16_t *) data)[3]);

  /* real frame number is set on the metadata superclass deserialize method */
  return (ExtractedMetadata *) extracted_object_bounding_box_new(0, x, y, width, height);
}

static int extracted_object_bounding_box_get_serialized_size(ExtractedMetadata * metadata)
{
    return sizeof(uint32_t) + sizeof(uint16_t) * 4;
}

static void extracted_object_bounding_box_save(ExtractedMetadata * metadata, int fd)
{
    ExtractedObjectBoundingBox * bounding_box = (ExtractedObjectBoundingBox *) metadata;
    char * message = NULL;
    size_t written = 0;
    size_t message_size = 0;

    if (asprintf(&message, "Object id[%u] x[%u] y[%u] width[%u] height[%u]\n", 
             bounding_box->id, 
             bounding_box->x, 
             bounding_box->y, 
             bounding_box->width, 
             bounding_box->height) == -1) {
        printf("extracted_object_bounding_box_save: ERROR ALLOCATING MESSAGE !!!\n");
        return;
    }

    message_size = strlen(message);
    written = write(fd, message, message_size);

    if (written != message_size) {
        printf("extracted_object_bounding_box_save: WARNING, expected to write[%d] but written only [%d]", message_size, written);
    }
    free(message);
}


/*
 ********************************
 * ExtractedYImage Public API *
 ********************************
 */

static void extracted_y_image_free(ExtractedMetadata * metadata);
static void extracted_y_image_serialize (ExtractedMetadata * metadata, char * data);
static int extracted_y_image_get_serialized_size(ExtractedMetadata * metadata);
static void extracted_y_image_save(ExtractedMetadata * metadata, int fd);

ExtractedYImage * extracted_y_image_new(unsigned int frame_num, int width, int height)
{
  ExtractedYImage * metadata = malloc(sizeof(ExtractedYImage));
  unsigned char ** y_rows   = malloc(sizeof(unsigned char *) * height);
  unsigned char * y_plane   = malloc(sizeof(unsigned char) * height * width);
  int row_offset            = 0;
  int row;

  /* Filling the rows */
  for (row = 0; row < height; row++) {
    /* y[0] = y_plane + 0. So y[0] points to the entire allocated plane */
    y_rows[row] = y_plane + row_offset;
    row_offset += width;
  }

  metadata->height = height;
  metadata->width  = width;
  metadata->y      = y_rows;
  
  extracted_metadata_init((ExtractedMetadata *) metadata,
                          extracted_y_image_free,
                          extracted_y_image_serialize,
                          extracted_y_image_get_serialized_size,
                          extracted_y_image_save,
                          ExtractedMetadataYImage,
                          frame_num);

  return metadata;
}

unsigned char ** extracted_y_image_get_y(ExtractedYImage * img)
{
  return img->y;
}


/*
 ************************************
 * ExtractedYImage private functions *
 ************************************
 */

static void extracted_y_image_free(ExtractedMetadata * metadata)
{
  /* metadata->y[0] points to the begin of the y plane. Easy to free all data */
  ExtractedYImage * img = (ExtractedYImage *) metadata;

  /* freeing the image plane */
  free(img->y[0]);

  /* freeing the rows array */
  free(img->y);

  free(img);
}

static ExtractedMetadata * extracted_y_image_deserialize(const char * data, int size)
{
  uint16_t width;
  uint16_t height;
  int plane_size;
  ExtractedYImage * img = NULL; 

  if (size < (sizeof(uint16_t) * 2)) {
    printf("extracted_y_image_deserialize: invalid serialized ExtractedYImage !!!\n");
    return NULL;
  }

  /* avoid problems with type size and endianness */
  width = ntohs(*((uint16_t *) data));
  data += sizeof(uint16_t);

  /* avoid problems with type size and endianness */
  height = ntohs(*((uint16_t *) data));
  data += sizeof(uint16_t);

  size -= sizeof(uint16_t) * 2;

  plane_size = width * height * sizeof(unsigned char);

  if (size < plane_size) {
    /* For some kind of reasom the JM software insert a byte on the end of the SEI message. */
    printf("extracted_y_image_deserialize: expected plane_size[%d] but was [%d]!!!\n", plane_size, size);
    return NULL;
  }

  /* real frame number is set on the metadata superclass deserialize method */
  img   = extracted_y_image_new(0, width, height);

  /* lets fill the plane */
  memcpy(img->y[0], data, width * height * sizeof(unsigned char));

  return (ExtractedMetadata *) img;
}

static void extracted_y_image_serialize (ExtractedMetadata * metadata, char * data)
{
  ExtractedYImage * img = (ExtractedYImage *) metadata;

  /* avoid problems with type size and endianness */
  *((uint16_t *) data) = htons(img->width);
  data += sizeof(uint16_t);

  *((uint16_t *) data) = htons(img->height);
  data += sizeof(uint16_t);

  /* Lets copy all the plane. Pretty easy since y[0] points to the begin of the plane */
  memcpy(data, img->y[0], img->width * img->height * sizeof(unsigned char));
}

static int extracted_y_image_get_serialized_size(ExtractedMetadata * metadata)
{
  ExtractedYImage * img = (ExtractedYImage *) metadata;
  return sizeof(uint16_t) + sizeof(uint16_t) + (img->width * img->height * sizeof(unsigned char));
}

static void extracted_y_image_save(ExtractedMetadata * metadata, int fd)
{
  /* We are not going to use OpenCV to save the image to a file because he messes 
     up some of the images while gimp can open the raw images easily */
  ExtractedYImage * img     = (ExtractedYImage *) metadata;
  int row;

  /* We must write R G B with the same Y sample. Forming a grayscale image */
  for (row = 0; row < img->height; row++) {
    size_t count   = sizeof(unsigned char) * img->width;
    size_t written = write(fd, img->y[row], count);
    
    if (written != count) {
      printf("Error writing output file fd[%d], written[%d] but expected[%d] !!!", fd, written, count);
      break;
    }
  }
}

/*
 *******************************
 * ExtractedMetadataBuffer API *
 *******************************
 */

/* */
static const short METADATA_BUFFER_SIZE  = 255; /* 2 ^ 8 = 0 <-> 255 = 0x000000FF */

/* Lets use a always empty slot technique to implement the ringbuffer */
static short extracted_metadata_buffer_get_next_index(short index)
{
  return (index + 1) & METADATA_BUFFER_SIZE;
}


ExtractedMetadataBuffer * extracted_metadata_buffer_new()
{
  ExtractedMetadataBuffer * buffer = malloc(sizeof(ExtractedMetadataBuffer));

  buffer->ringbuffer  = malloc(sizeof(ExtractedMetadata *) * METADATA_BUFFER_SIZE);
  buffer->read_index  = 0;
  buffer->write_index = 0;

  return buffer;
}


void extracted_metadata_buffer_add(ExtractedMetadataBuffer * buffer, ExtractedMetadata * obj)
{
  short new_write_index = extracted_metadata_buffer_get_next_index(buffer->write_index);

  if (!buffer || !obj) {
    printf("extracted_metadata_buffer_add: ERROR: NULL parameters given !!!\n");
    return;
  }

  if (new_write_index == buffer->read_index) {
    printf("extracted_metadata_buffer_add: ERROR: BUFFER OVERFLOW !!!\n");
    return;
  }

  buffer->ringbuffer[buffer->write_index] = obj;
  buffer->write_index                     = new_write_index;
}

ExtractedMetadata * extracted_metadata_buffer_get(ExtractedMetadataBuffer * buffer, unsigned int frame_number)
{
  ExtractedMetadata * obj = NULL;

  while (buffer->write_index != buffer->read_index) {
    obj                = buffer->ringbuffer[buffer->read_index];
    buffer->read_index = extracted_metadata_buffer_get_next_index(buffer->read_index);

    if (frame_number == obj->frame_number) {
      return obj;
    }
    
    extracted_metadata_free(obj);
  }

  printf("extracted_metadata_buffer_get: cant find frame_number [%u]\n", frame_number);
  return NULL;
}

