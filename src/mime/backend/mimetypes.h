/* $Id: mimetypes.h,v 1.3 2004/01/01 15:47:25 jonas Exp $ */

#ifndef EL__MIME_BACKEND_MIMETYPES_H
#define EL__MIME_BACKEND_MIMETYPES_H

#ifdef CONFIG_MIMETYPES

#include "mime/backend/common.h"
#include "modules/module.h"

extern struct mime_backend mimetypes_mime_backend;
extern struct module mimetypes_mime_module;

#endif
#endif
