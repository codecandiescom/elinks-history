/* $Id: common.h,v 1.7 2003/06/06 21:18:07 jonas Exp $ */

#ifndef EL__MIME_BACKEND_COMMON_H
#define EL__MIME_BACKEND_COMMON_H

#include <stdio.h>

#include "mime/mime.h"
#include "util/lists.h"

struct mime_backend {
	/* String to identify the backend. */
	unsigned char *name;

	/* Startup and teardown. */
	void (*init)(void);
	void (*done)(void);

	/* Given an @uri resolve the content type. For know it (mostly)
	 * comes down to using the the extension to resolve. */
	unsigned char *(*get_content_type)(unsigned char *uri);

	/* Given a mime type find a associated handler. The handler can
	 * be given options such */
	struct mime_handler *(*get_mime_handler)(unsigned char *type, int have_x);
};

/* Multiplexor functions for the backends. */

void init_mime_backends(void);

void done_mime_backends(void);

unsigned char *get_content_type_backends(unsigned char *uri);

struct mime_handler *
get_mime_handler_backends(unsigned char *content_type, int have_x);

/* TODO Maybe this could fit in util/file.h */
/* Extracts a filename from @path separated by @separator. Targeted for use
 * with the general unix PATH style strings. */
unsigned char *
get_next_path_filename(unsigned char **path_ptr, unsigned char separator);

/* TODO Temporarily put here until mime/ will be integrated (I hope ;) */
int get_extension_from_url(unsigned char *url, unsigned char **extension);

#endif
