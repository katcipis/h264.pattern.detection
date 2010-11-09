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

void user_data_parser_unregistered_sei(byte* payload, int size);

#endif
