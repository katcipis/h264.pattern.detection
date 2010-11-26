/*!
 *******************************************************************************
 *  \file
 *     udata_gen.h
 *  \brief
 *     definitions for Supplemental Enhanced Information Userdata Generation
 *  \author(s)
 *      - Tiago Katcipis                             <tiagokatcipis@gmail.com>
 *
 * *****************************************************************************
 */

#ifndef UDATA_GEN_H
#define UDATA_GEN_H

#include "typedefs.h"
#include "nal.h"

/*!
 *****************************************************************************************
 * Generates a SEI NALU that with ONE unregistered userdata SEI message.
 *
 * @param data The data to be sent on the SEI unregistered userdata message.
 * @param size The size of the data.
 * @return A SEI NALU containing the SEI message.
 *
 *****************************************************************************************
 */
NALU_t *user_data_generate_unregistered_sei_nalu(char * data, unsigned int size);

/*!
 *****************************************************************************************
 * Generates a SEI NALU that with ONE unregistered userdata SEI message.
 *
 * @param msg The message to be sent on the SEI unregistered userdata message, 
 *            must be a 0 terminated string.
 * @return A SEI NALU containing the SEI message.
 *
 ******************************************************************************************
 */
NALU_t *user_data_generate_unregistered_sei_nalu_from_msg(char * msg);

/*!
 *******************************************************************************************
 * Generates a 0 terminated string to be sent on a SEI NALU. The size of the message is
 * random, but it will not exceed MAXNALUSIZE.
 *
 * @return a 0 terminated string.
 *
 ********************************************************************************************
 */
char* user_data_generate_create_random_message();

/*!
 *******************************************************************************************
 * Frees the resources used by a msg.
 *
 * @param msg A previously created message.
 *
 ********************************************************************************************
 */
void user_data_generate_destroy_random_message(char * msg);

#endif
