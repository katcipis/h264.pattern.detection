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

#endif
