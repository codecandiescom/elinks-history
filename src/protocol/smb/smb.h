/* $Id: smb.h,v 1.4 2004/05/07 01:46:09 jonas Exp $ */

#ifndef EL__PROTOCOL_SMB_SMB_H
#define EL__PROTOCOL_SMB_SMB_H

#include "protocol/protocol.h"

#ifdef CONFIG_SMB
extern struct protocol_backend smb_protocol_backend;
#else
#define smb_protocol_backend unknown_protocol_backend
#endif

#endif
