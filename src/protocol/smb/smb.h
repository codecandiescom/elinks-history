/* $Id: smb.h,v 1.5 2004/05/07 17:27:46 jonas Exp $ */

#ifndef EL__PROTOCOL_SMB_SMB_H
#define EL__PROTOCOL_SMB_SMB_H

#include "protocol/protocol.h"

#ifdef CONFIG_SMB
extern protocol_handler smb_protocol_handler;
#else
#define smb_protocol_handler NULL
#endif

#endif
