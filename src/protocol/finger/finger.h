/* $Id: finger.h,v 1.3 2005/03/05 21:04:49 jonas Exp $ */

#ifndef EL__PROTOCOL_FINGER_FINGER_H
#define EL__PROTOCOL_FINGER_FINGER_H

#include "modules/module.h"
#include "protocol/protocol.h"

#ifdef CONFIG_FINGER
extern protocol_handler_T finger_protocol_handler;
#else
#define finger_protocol_handler NULL
#endif

extern struct module finger_protocol_module;


#endif
