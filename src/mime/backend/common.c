/* MIME handling backends multiplexing */
/* $Id: common.c,v 1.1 2003/05/06 17:24:40 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "mime/backend/common.h"
#include "mime/mime.h"
#include "util/memory.h"
#include "util/string.h"


/* Backends dynamic area: */

#include "mime/backend/default.h"
#include "mime/backend/mailcap.h"

/* Note that the numbering is static, that means that you have to provide at
 * least dummy NULL handlers even when no support is compiled in. */

static struct mime_backend *mime_backends[] = {
	&default_mime_backend,
	&mailcap_mime_backend,
	NULL
};


void
init_mime_backends()
{
	int backend_index = 0;

	for (; mime_backends[backend_index]; backend_index++) {
		struct mime_backend *backend = mime_backends[backend_index];

		if (backend->init)
			backend->init();
	}
}

void
done_mime_backends()
{
	int backend_index = 0;

	for (; mime_backends[backend_index]; backend_index++) {
		struct mime_backend *backend = mime_backends[backend_index];

		if (backend->done)
			backend->done();
	}
}

/* TODO Make backend selection scheme configurable */

unsigned char *
get_content_type_backends(unsigned char *uri)
{
	int backend_index = 0;

	for (; mime_backends[backend_index]; backend_index++) {
		struct mime_backend *backend = mime_backends[backend_index];

		if (backend->get_content_type) {
			unsigned char *content_type;

			content_type = backend->get_content_type(uri);
			if (content_type) return content_type;
		}
	}

	return NULL;
}

struct mime_handler *
get_mime_handler_backends(unsigned char *uri, int have_x)
{
	int backend_index = 0;

	for (; mime_backends[backend_index]; backend_index++) {
		struct mime_backend *backend = mime_backends[backend_index];

		if (backend->get_mime_handler) {
			struct mime_handler *handler;

			handler = backend->get_mime_handler(uri, have_x);
			if (handler) return handler;
		}
	}

	return NULL;
}
