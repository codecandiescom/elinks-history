/* $Id: mime.h,v 1.7 2003/06/18 00:30:25 jonas Exp $ */

#ifndef EL__MIME_MIME_H
#define EL__MIME_MIME_H

#include "config/options.h"

struct mime_handler {
	unsigned char *program;
	unsigned char *description;
	unsigned char *backend_name;
	unsigned int ask:1;
	unsigned int block:1;
};

/* Start up and teardown of mime system. */
void init_mime(void);
void done_mime(void);

/* Guess content type of the document. Either from the (http) @header or
 * using the @uri (extension). */
unsigned char *get_content_type(unsigned char *header, unsigned char *uri);

/* Find program to handle mimetype. The @xwin tells about X capabilities. */
struct mime_handler *
get_mime_type_handler(unsigned char *content_type, int xwin);

#endif
