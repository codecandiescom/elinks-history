/* $Id: mailcap.h,v 1.4 2004/01/01 15:47:25 jonas Exp $ */

#ifndef EL__MIME_BACKEND_MAILCAP_H
#define EL__MIME_BACKEND_MAILCAP_H

#ifdef CONFIG_MAILCAP

#include "mime/backend/common.h"
#include "modules/module.h"

extern struct mime_backend mailcap_mime_backend;
extern struct module mailcap_mime_module;

#endif
#endif
