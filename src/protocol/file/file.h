/* $Id: file.h,v 1.7 2004/01/24 22:45:25 jonas Exp $ */

#ifndef EL__PROTOCOL_FILE_FILE_H
#define EL__PROTOCOL_FILE_FILE_H

#include "protocol/protocol.h"
#include "sched/connection.h"
struct string;

extern struct protocol_backend file_protocol_backend;

/* Reads the file with the given @filename into the string @source. */
enum connection_state read_encoded_file(unsigned char *filename, int filenamelen, struct string *source);

#endif
