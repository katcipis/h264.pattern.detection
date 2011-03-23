#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "udata_gen.h"
#include "vlc.h"
#include "sei.h"
#include "nalu.h"


static const char* random_message_start_template = "\nRandom message[%d] start\n";
static const char* random_message_end_template   = "\nRandom message[%d] end!\n";
static int msg_counter                           = 1;

/* Lets guarantee that max_msg_size + headers + 
   emulation prevention bytes dont exceed the MAXNALUSIZE */
static const int max_msg_size                    = MAXNALUSIZE - 1024; 

static int growth_rate                           = 1024;
static int actual_size                           = 1024;

#define MAX_TEMPLATE_MSG_SIZE 512
static char random_message_start_buffer[MAX_TEMPLATE_MSG_SIZE];
static char random_message_end_buffer[MAX_TEMPLATE_MSG_SIZE]; 

/*!
 *************************************************************************************
 * \brief
 *    int GenerateUserDataSEImessage_rbsp(int, byte*, char*, unsigned int)
 *
 * \return
 *    size of the RBSP in bytes, negative in case of an error
 *
 *************************************************************************************
 */
static int GenerateUserDataSEImessage_rbsp (int id, byte *rbsp, char* sei_message, unsigned int sei_message_size)
{
  Bitstream *bitstream;
  unsigned int message_size = sei_message_size;

  int LenInBytes;
  assert (rbsp != NULL);

  if ((bitstream=calloc(1, sizeof(Bitstream)))==NULL)
    no_mem_exit("SeqParameterSet:bitstream");

  // .. and use the rbsp provided (or allocated above) for the data
  bitstream->streamBuffer = rbsp;
  bitstream->bits_to_go = 8;

  {
    char uuid_message[9] = "Random"; // This is supposed to be Random
    unsigned int i;

    TIME_T start_time;
    gettime(&start_time);    // start time

    u_v (8,"SEI: last_payload_type_byte", 5, bitstream);
    message_size += 17;
    while (message_size > 254)
    {
      u_v (8,"SEI: ff_byte",255, bitstream);
      message_size -= 255;
    }
    u_v (8,"SEI: last_payload_size_byte",message_size, bitstream);

    // Lets randomize uuid based on time
    u_v (32,"SEI: uuid_iso_iec_11578",(int) start_time.tv_sec, bitstream);
    u_v (32,"SEI: uuid_iso_iec_11578",(int) start_time.tv_usec, bitstream);

    u_v (32,"SEI: uuid_iso_iec_11578",(int) (uuid_message[0] << 24) + (uuid_message[1] << 16)  + (uuid_message[2] << 8) + (uuid_message[3] << 0), bitstream);
    u_v (32,"SEI: uuid_iso_iec_11578",(int) (uuid_message[4] << 24) + (uuid_message[5] << 16)  + (uuid_message[6] << 8) + (uuid_message[7] << 0), bitstream);

    for (i = 0; i < sei_message_size; i++) {
        u_v (8,"SEI: user_data_payload_byte",sei_message[i], bitstream);
    }

    /* FIXME we MUST have this zero or the original coded was suposed to zero terminate the msg ? */
    u_v (8,"SEI: user_data_payload_byte", 0, bitstream);
  }

  SODBtoRBSP(bitstream);     // copies the last couple of bits into the byte buffer
  LenInBytes=bitstream->byte_pos;

  free(bitstream);
  return LenInBytes;
}


/*!
 *************************************************************************************
 * \brief
 *    Function body for Unregistered userdata SEI message NALU generation
 *
 * \return
 *    A NALU containing the SEI message.
 *
 *************************************************************************************
 */
NALU_t * user_data_generate_unregistered_sei_nalu(char * data, unsigned int size)
{
  if (size > MAXNALUSIZE) {
    error("user_data_generate_unregistered_sei_nalu: Trying to generate a NALU with a size bigger than MAXNALUSIZE", 500);
  }

  NALU_t *n = AllocNALU(MAXNALUSIZE);
  int RBSPlen = 0;
  byte rbsp[MAXRBSPSIZE];

  RBSPlen = GenerateUserDataSEImessage_rbsp (NORMAL_SEI, rbsp, data, size);
  RBSPtoNALU (rbsp, n, RBSPlen, NALU_TYPE_SEI, NALU_PRIORITY_DISPOSABLE, 1);

  n->startcodeprefix_len = 4;
  return n;
}


/*!
 *************************************************************************************
 * \brief
 *    Function body for Unregistered userdata SEI message NALU generation
 *
 * \return
 *    A NALU containing the SEI message.
 *
 *************************************************************************************
 */
NALU_t *user_data_generate_unregistered_sei_nalu_from_msg(char * msg)
{
    return user_data_generate_unregistered_sei_nalu(msg, strlen(msg));
}


/*!
 *************************************************************************************
 * \brief
 *    Function body for random string message generation.
 *
 * \return
 *    A 0 terminted string.
 *
 *************************************************************************************
 */
char* user_data_generate_create_random_message()
{
    char * msg    = 0;
    int msg_size  = 0;
    int body_size = 0;
    int start_len = 0;
    int end_len   = 0;
    int i         = 0;

    /* This will help debug of the SEI messages at the decoder */
    snprintf (random_message_start_buffer, MAX_TEMPLATE_MSG_SIZE, random_message_start_template, msg_counter);
    snprintf (random_message_end_buffer, MAX_TEMPLATE_MSG_SIZE, random_message_end_template, msg_counter);
    msg_counter++;

    start_len = strlen(random_message_start_buffer);
    end_len  = strlen(random_message_end_buffer);

    body_size = actual_size;
    actual_size += growth_rate;

    if (actual_size > max_msg_size) {
        actual_size = growth_rate;
    }
    
    msg_size = body_size + start_len +  end_len + 1;
    msg      = malloc(msg_size);

    /* writing the start */
    memcpy(msg, random_message_start_buffer, start_len);
    
    /* writing the body */
    for (i = start_len; i < msg_size - end_len; i++) {
        msg[i] = 'h'; /* whatever */
    }

    /* writing the end */
    memcpy(msg + start_len + body_size, random_message_end_buffer, end_len);

    /* 0 terminate it */
    msg[msg_size - 1] = '\0';
    return msg;
}

/*!
 *************************************************************************************
 * \brief
 *    Function body for random message destruction.
 *************************************************************************************
 */
void user_data_generate_destroy_random_message(char * msg)
{
    free(msg);
}
