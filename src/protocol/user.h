/* $Id: user.h,v 1.6 2003/06/26 20:04:40 jonas Exp $ */

#ifndef EL__PROTOCOL_USER_H
#define EL__PROTOCOL_USER_H

#include "protocol/protocol.h"
#include "terminal/terminal.h"

extern struct protocol_backend user_protocol_backend;

unsigned char *get_prog(struct terminal *, unsigned char *);

#endif
