/* $Id: mime.h,v 1.5 2003/06/07 22:18:20 jonas Exp $ */

#ifndef EL__MIME_MIME_H
#define EL__MIME_MIME_H

#include "config/options.h"
#include "terminal/terminal.h"

struct mime_handler {
	int flags;
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

/* Find program to handle mimetype. The @term is for getting info about X
 * capabilities. */
struct mime_handler *
get_mime_type_handler(struct terminal *term, unsigned char *content_type);

#endif
