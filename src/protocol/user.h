/* $Id: user.h,v 1.7 2003/07/08 18:46:40 jonas Exp $ */

#ifndef EL__PROTOCOL_USER_H
#define EL__PROTOCOL_USER_H

#include "protocol/protocol.h"
#include "terminal/terminal.h"

extern struct protocol_backend user_protocol_backend;

unsigned char *get_user_program(struct terminal *, unsigned char *, int);

#endif
