/* $Id: finger.h,v 1.7 2004/05/09 23:04:40 jonas Exp $ */

#ifndef EL__PROTOCOL_FINGER_H
#define EL__PROTOCOL_FINGER_H

#include "protocol/protocol.h"

#ifdef CONFIG_FINGER
extern protocol_handler finger_protocol_handler;
#else
#define finger_protocol_handler NULL
#endif

#endif
