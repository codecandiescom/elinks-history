/* $Id: common.h,v 1.3 2003/06/04 18:39:01 jonas Exp $ */

#ifndef EL__MIME_BACKEND_COMMON_H
#define EL__MIME_BACKEND_COMMON_H

#include <stdio.h>
#include "util/lists.h"
#include "mime/mime.h"

struct mime_backend {
	/* Startup and teardown. */
	void (*init)();
	void (*done)();

	/* Given an extension resolve the type. */
	unsigned char *(*get_content_type)(unsigned char *uri);

	/* Given a mime type find a associated handler. The handler can
	 * be given options such */
	struct mime_handler *(*get_mime_handler)(unsigned char *type, int options);
};

/* Multiplexor functions for the backends. */
void init_mime_backends();

void done_mime_backends();

unsigned char *get_content_type_backends(unsigned char *uri);

struct mime_handler *get_mime_handler_backends(unsigned char *uri, int have_x);

/* TODO Maybe this could maybe fit in util/file.h */
/* Extracts a filename from @path separated by @separator. Targeted for use
 * with the general unix PATH style strings. */
unsigned char *
get_next_path_filename(unsigned char **path_ptr, unsigned char separator);

/* This is only temporarily put here until mime/ will be integrated (I hope) */
int get_extension_from_url(unsigned char *url, unsigned char **extension);

#endif
