/* $Id: mime.h,v 1.11 2004/03/22 14:35:39 jonas Exp $ */

#ifndef EL__MIME_MIME_H
#define EL__MIME_MIME_H

#include "config/options.h"
#include "modules/module.h"

struct mime_handler {
	unsigned char *description;
	unsigned char *backend_name;
	unsigned int ask:1;
	unsigned int block:1;
	unsigned char program[1]; /* XXX: Keep last! */
};

extern struct module mime_module;

/* Guess content type of the document. Either from the (http) @header or
 * using the @uri (extension). */
unsigned char *get_content_type(unsigned char *header, unsigned char *uri);

/* Find program to handle mimetype. The @xwin tells about X capabilities. */
struct mime_handler *
get_mime_type_handler(unsigned char *content_type, int xwin);

#endif
