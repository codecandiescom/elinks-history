/* $Id: mime.h,v 1.2 2003/06/04 19:26:22 jonas Exp $ */

#ifndef EL__MIME_MIME_H
#define EL__MIME_MIME_H

#include "config/options.h"
#include "terminal/terminal.h"

enum mime_handler_flags {
	MIME_ASK	= (1 << 0),
	MIME_BLOCK	= (1 << 1),
};

struct mime_handler {
	int flags;
	unsigned char *program;
	unsigned char *description;
	unsigned char *backend_name;
};

/* Start up and teardown of mime system. */
void init_mime();
void done_mime();

/* Guess content type of the document. Either from the (http) @header or
 * using the @uri (extension). */
unsigned char *get_content_type(unsigned char *header, unsigned char *uri);

/* Find program to handle mimetype. The @term is for getting info about X
 * capabilities. */
struct mime_handler *
get_mime_type_handler(struct terminal *term, unsigned char *content_type);

#endif
