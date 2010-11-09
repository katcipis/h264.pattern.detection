/*!
 ************************************************************************
 *  \file
 *     udata_gen.h
 *  \brief
 *     definitions for Supplemental Enhanced Information Userdata Generation
 *  \author(s)
 *      - Tiago Katcipis                             <tiagokatcipis@gmail.com>
 *
 * ************************************************************************
 */

#ifndef UDATA_GEN_H
#define UDATA_GEN_H

#include "typedefs.h"
#include "nal.h"

/*!
 *************************************************************************************
 * Generated NALU that with a unregistered userdata SEI message with random data.
 *
 * @param data The data to be sent on the SEI message.
 * @param size The size of the data.
 * @return A NALU containing the SEI message.
 *
 *************************************************************************************
 */
NALU_t *user_data_generate_unregistered_sei_nalu(char * data, unsigned int size);

#endif
