/* $Id: common.h,v 1.1 2003/05/06 17:24:40 jonas Exp $ */

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

struct mime_handler *
get_mime_handler_backends(unsigned char *uri, int have_x);

#endif
