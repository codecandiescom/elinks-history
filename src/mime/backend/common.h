/* $Id: common.h,v 1.12 2003/06/10 21:19:11 jonas Exp $ */

#ifndef EL__MIME_BACKEND_COMMON_H
#define EL__MIME_BACKEND_COMMON_H

#include <stdio.h>

#include "mime/mime.h"
#include "util/lists.h"

struct mime_backend {
	LIST_HEAD(struct mime_backend);

	/* String to identify the backend. */
	unsigned char *name;

	/* Startup and teardown. */
	void (*init)(void);
	void (*done)(void);

	/* Resolve the content type from the @extension. */
	unsigned char *(*get_content_type)(unsigned char *extension);

	/* Given a mime type find a associated handler. The handler can
	 * be given options such */
	struct mime_handler *(*get_mime_handler)(unsigned char *type, int have_x);
};

/* Multiplexor functions for the backends. */

void init_mime_backends(void);

void done_mime_backends(void);

unsigned char *get_content_type_backends(unsigned char *extension);

struct mime_handler *
get_mime_handler_backends(unsigned char *content_type, int have_x);

/* TODO Temporarily put here until we move it to util/ and protocol/url.c */

/* Extracts a filename from @path separated by @separator. Targeted for use
 * with the general unix PATH style strings. */
unsigned char *
get_next_path_filename(unsigned char **path_ptr, unsigned char separator);

/* Returns allocated string containing the biggest possible extension.
 * If url is 'jabadaba.1.foo.gz' the extensionpart is '1.foo.gz' */
unsigned char *get_extensionpart_from_url(unsigned char *url);

#endif
