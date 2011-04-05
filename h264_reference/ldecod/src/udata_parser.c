#include "udata_parser.h"
#include <stdio.h>

/* 16 bytes - 128bits - According to ITU-T REC 03/2010 - Pag 327 */
static const int UUID_ISO_IEC_OFFSET = 16;


void user_data_parser_unregistered_sei_dump( byte* payload, int size)
{
  int offset = 0;
  byte payload_byte;

  printf("\nUser data unregistered SEI message, size[%d]\n", size);
  printf("uuid_iso_11578 = 0x");
  
  assert (size>=UUID_ISO_IEC_OFFSET);

  for (offset = 0; offset < UUID_ISO_IEC_OFFSET; offset++)
  {
    printf("%02x",payload[offset]);
  }

  printf("\nUser data unregistered SEI message start\n");

  while (offset < size)
  {
    payload_byte = payload[offset];
    offset ++;
    printf("%c", payload_byte);
  }

  printf("\nUser data unregistered SEI message end \n");
}

void user_data_parser_unregistered_sei_get_data(byte* payload, int size, byte ** outdata, int * outsize)
{
  assert (size>=UUID_ISO_IEC_OFFSET);

  *outdata = payload + UUID_ISO_IEC_OFFSET;
  *outsize = size - UUID_ISO_IEC_OFFSET;
}
