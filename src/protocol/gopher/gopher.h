/* $Id: gopher.h,v 1.1 2004/08/18 17:24:19 jonas Exp $ */

#ifndef EL__PROTOCOL_GOPHER_GOPHER_H
#define EL__PROTOCOL_GOPHER_GOPHER_H

#include "protocol/protocol.h"

#ifdef CONFIG_GOPHER
extern protocol_handler gopher_protocol_handler;
#else
#define gopher_protocol_handler NULL
#endif

#endif
