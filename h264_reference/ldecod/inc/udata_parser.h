/*!
 ************************************************************************
 *  \file
 *     udata_parser.h
 *  \brief
 *     definitions for Supplemental Enhanced Information Userdata Parsing
 *  \author(s)
 *      - Tiago Katcipis                             <tiagokatcipis@gmail.com>
 *
 * ************************************************************************
 */

#ifndef UDATA_PARSER_H
#define UDATA_PARSER_H

#include "typedefs.h"

/*!
 *******************************************************************************
 * Parses a SEI User Data Unregistered message, dumping all its data at stdout.
 *
 * @param payload The payload of the SEI message.
 * @param size The size of the payload.
 *
 *******************************************************************************
 */
void user_data_parser_unregistered_sei(byte* payload, int size);

/*!
 *******************************************************************************
 * Parses a SEI User Data Unregistered message, getting its content without 
 * the uuid_iso_iec_11578 bytes.
 *
 * @param payload The payload of the SEI message.
 * @param size The size of the payload.
 * @param outdata The address of the pointer that will be assigned to the data. 
 * @param outsize The size of the data. 
 * @return outdata and outsize, data should not be freed since it is not a copy 
 *         of the payload. It is only valid as long as the payload is valid.
 *
 *******************************************************************************
 */
void user_data_parser_unregistered_sei_get_data(byte* payload, int size, byte ** outdata, int * outsize);

#endif
