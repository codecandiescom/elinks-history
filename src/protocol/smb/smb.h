/* $Id: smb.h,v 1.2 2004/01/01 10:04:04 jonas Exp $ */

#ifndef EL__PROTOCOL_SMB_SMB_H
#define EL__PROTOCOL_SMB_SMB_H

#include "protocol/protocol.h"

#ifdef CONFIG_SMB
extern struct protocol_backend smb_protocol_backend;
#endif

#endif
