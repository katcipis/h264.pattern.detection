#include "udata_gen.h"
#include "vlc.h"
#include "sei.h"
#include "nalu.h"


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
static int GenerateUserDataSEImessage_rbsp (int id, byte *rbsp, char* sei_message, unsigned int message_size)
{
  Bitstream *bitstream;

  int len = 0, LenInBytes;
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

    len+=u_v (8,"SEI: last_payload_type_byte", 5, bitstream);
    message_size += 17;
    while (message_size > 254)
    {
      len+=u_v (8,"SEI: ff_byte",255, bitstream);
      message_size -= 255;
    }
    len+=u_v (8,"SEI: last_payload_size_byte",message_size, bitstream);

    // Lets randomize uuid based on time
    len+=u_v (32,"SEI: uuid_iso_iec_11578",(int) start_time.tv_sec, bitstream);
    len+=u_v (32,"SEI: uuid_iso_iec_11578",(int) start_time.tv_usec, bitstream);

    len+=u_v (32,"SEI: uuid_iso_iec_11578",(int) (uuid_message[0] << 24) + (uuid_message[1] << 16)  + (uuid_message[2] << 8) + (uuid_message[3] << 0), bitstream);
    len+=u_v (32,"SEI: uuid_iso_iec_11578",(int) (uuid_message[4] << 24) + (uuid_message[5] << 16)  + (uuid_message[6] << 8) + (uuid_message[7] << 0), bitstream);

    for (i = 0; i < strlen(sei_message); i++) {
        len+=u_v (8,"SEI: user_data_payload_byte",sei_message[i], bitstream);
    }

    len+=u_v (8,"SEI: user_data_payload_byte", 0, bitstream);
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
  NALU_t *n = AllocNALU(MAXNALUSIZE);
  int RBSPlen = 0;
  int NALUlen;
  byte rbsp[MAXRBSPSIZE];

  RBSPlen = GenerateUserDataSEImessage_rbsp (NORMAL_SEI, rbsp, data, size);
  NALUlen = RBSPtoNALU (rbsp, n, RBSPlen, NALU_TYPE_SEI, NALU_PRIORITY_DISPOSABLE, 1);

  n->startcodeprefix_len = 4;
  return n;
}

