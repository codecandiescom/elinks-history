/* $Id: https.h,v 1.9 2004/05/07 17:47:05 jonas Exp $ */

#ifndef EL__PROTOCOL_HTTP_HTTPS_H
#define EL__PROTOCOL_HTTP_HTTPS_H

#include "protocol/protocol.h"

#ifdef CONFIG_SSL
extern protocol_handler https_protocol_handler;
#else
#define https_protocol_handler NULL
#endif

#endif
