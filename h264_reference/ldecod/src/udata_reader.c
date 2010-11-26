#include "udata_parser.h"
#include <stdio.h>

#define UUID_ISO_IEC_OFFSET 16 


void user_data_parser_unregistered_sei( byte* payload, int size)
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
