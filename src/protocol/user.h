/* $Id: user.h,v 1.8 2004/05/07 17:27:46 jonas Exp $ */

#ifndef EL__PROTOCOL_USER_H
#define EL__PROTOCOL_USER_H

#include "protocol/protocol.h"
#include "terminal/terminal.h"

extern protocol_external_handler user_protocol_handler;

unsigned char *get_user_program(struct terminal *, unsigned char *, int);

#endif
