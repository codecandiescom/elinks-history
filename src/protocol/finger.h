/* $Id: finger.h,v 1.8 2004/10/08 17:19:25 zas Exp $ */

#ifndef EL__PROTOCOL_FINGER_H
#define EL__PROTOCOL_FINGER_H

#include "modules/module.h"
#include "protocol/protocol.h"

#ifdef CONFIG_FINGER
extern protocol_handler finger_protocol_handler;
#else
#define finger_protocol_handler NULL
#endif

extern struct module finger_protocol_module;


#endif
