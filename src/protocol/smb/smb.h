/* $Id: smb.h,v 1.7 2005/03/05 21:04:49 jonas Exp $ */

#ifndef EL__PROTOCOL_SMB_SMB_H
#define EL__PROTOCOL_SMB_SMB_H

#include "modules/module.h"
#include "protocol/protocol.h"

extern struct module smb_protocol_module;

#ifdef CONFIG_SMB
extern protocol_handler_T smb_protocol_handler;
#else
#define smb_protocol_handler NULL
#endif

#endif
