/* $Id: mailcap.h,v 1.3 2003/10/25 19:17:32 jonas Exp $ */

#ifndef EL__MIME_BACKEND_MAILCAP_H
#define EL__MIME_BACKEND_MAILCAP_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mime/backend/common.h"
#include "modules/module.h"

#ifdef MAILCAP
extern struct mime_backend mailcap_mime_backend;
extern struct module mailcap_mime_module;
#endif

#endif
