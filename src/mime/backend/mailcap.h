/* $Id: mailcap.h,v 1.2 2003/06/03 23:20:21 jonas Exp $ */

#ifndef EL__MIME_BACKEND_MAILCAP_H
#define EL__MIME_BACKEND_MAILCAP_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mime/backend/common.h"

#ifdef MAILCAP
extern struct mime_backend mailcap_mime_backend;
#endif

#endif
