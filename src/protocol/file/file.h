/* $Id: file.h,v 1.9 2004/05/07 17:27:46 jonas Exp $ */

#ifndef EL__PROTOCOL_FILE_FILE_H
#define EL__PROTOCOL_FILE_FILE_H

#include "protocol/protocol.h"
struct string;

extern protocol_handler file_protocol_handler;

/* Reads the file with the given @filename into the string @source. */
enum connection_state read_encoded_file(unsigned char *filename, int filenamelen, struct string *source);

#endif
