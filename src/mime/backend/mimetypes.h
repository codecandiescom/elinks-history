/* $Id: mimetypes.h,v 1.2 2003/10/25 19:17:57 jonas Exp $ */

#ifndef EL__MIME_BACKEND_MIMETYPES_H
#define EL__MIME_BACKEND_MIMETYPES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "modules/module.h"

#ifdef MIMETYPES
extern struct mime_backend mimetypes_mime_backend;
extern struct module mimetypes_mime_module;
#endif

#endif
