/* $Id: finger.h,v 1.1 2004/10/12 15:28:12 zas Exp $ */

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
