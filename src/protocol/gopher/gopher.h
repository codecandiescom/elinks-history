/* $Id: gopher.h,v 1.3 2005/03/05 21:04:49 jonas Exp $ */

#ifndef EL__PROTOCOL_GOPHER_GOPHER_H
#define EL__PROTOCOL_GOPHER_GOPHER_H

#include "modules/module.h"
#include "protocol/protocol.h"

#ifdef CONFIG_GOPHER
extern protocol_handler_T gopher_protocol_handler;
#else
#define gopher_protocol_handler NULL
#endif

extern struct module gopher_protocol_module;


#endif
