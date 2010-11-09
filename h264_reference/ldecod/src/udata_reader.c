#include "udata_parser.h"
#include <stdio.h>

#define UUID_ISO_IEC_OFFSET 16 


void user_data_parser_unregistered_sei( byte* payload, int size)
{
  int offset = 0;
  byte payload_byte;

  printf("User data unregistered SEI message\n");
  printf("uuid_iso_11578 = 0x");
  
  assert (size>=UUID_ISO_IEC_OFFSET);

  for (offset = 0; offset < UUID_ISO_IEC_OFFSET; offset++)
  {
    printf("%02x",payload[offset]);
  }

  printf("\n");

  while (offset < size)
  {
    payload_byte = payload[offset];
    offset ++;
    printf("Unreg data payload_byte = %c\n", payload_byte);
  }
}
