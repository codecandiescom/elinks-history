/* $Id: user.h,v 1.10 2005/03/05 21:04:49 jonas Exp $ */

#ifndef EL__PROTOCOL_USER_H
#define EL__PROTOCOL_USER_H

#include "modules/module.h"
#include "protocol/protocol.h"
#include "terminal/terminal.h"

extern struct module user_protocol_module;
extern protocol_external_handler_T user_protocol_handler;

unsigned char *get_user_program(struct terminal *, unsigned char *, int);

#endif
