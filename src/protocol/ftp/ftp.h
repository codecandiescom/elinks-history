/* $Id: ftp.h,v 1.9 2005/03/05 21:04:49 jonas Exp $ */

#ifndef EL__PROTOCOL_FTP_FTP_H
#define EL__PROTOCOL_FTP_FTP_H

#include "modules/module.h"
#include "protocol/protocol.h"

extern struct module ftp_protocol_module;

#ifdef CONFIG_FTP
extern protocol_handler_T ftp_protocol_handler;
#else
#define ftp_protocol_handler NULL
#endif

#endif
