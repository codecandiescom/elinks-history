/* MIME handling backends multiplexing */
/* $Id: common.c,v 1.4 2003/06/04 18:39:01 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "mime/backend/common.h"
#include "mime/mime.h"
#include "protocol/url.h" /* For end_of_dir() */
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"


/* Backends dynamic area: */

#include "mime/backend/default.h"
#include "mime/backend/mailcap.h"

static struct mime_backend *mime_backends[] = {
	&default_mime_backend,
#ifdef MAILCAP
	&mailcap_mime_backend,
#endif
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

unsigned char *
get_next_path_filename(unsigned char **path_ptr, unsigned char separator)
{
	unsigned char *path = *path_ptr;
	unsigned char *filename = path;
	int filenamelen;

	/* Extract file from path */
	while (*path && *path != separator)
		path++;

	filenamelen = path - filename;

	/* If not at end of string skip separator */
	if (*path)
		path++;

	*path_ptr = path;

	if (filenamelen >= 0) {
		unsigned char *tmp = memacpy(filename, filenamelen);

		if (!tmp) return NULL;
		filename = expand_tilde(tmp);
		mem_free(tmp);
	} else {
		filename = NULL;
	}

	return filename;
}

int
get_extension_from_url(unsigned char *url, unsigned char **ext)
{
	int lo = !strncasecmp(url, "file://", 7); /* dsep() *hint* *hint* */
	unsigned char *extension = NULL;
	int extensionlen = 0;

#define dsep(x) (lo ? dir_sep(x) : (x) == '/')

	/* Hmmm, well, can we do better there ? --Zas */
	for (; *url && !end_of_dir(*url); url++) {
		if (*url == '.') {
			extension = url + 1;
		} else if (dsep(*url)) {
			extension = NULL;
		}
	}

	if (extension) {
		while (extension[extensionlen]
		       && !dsep(extension[extensionlen])
		       && !end_of_dir(extension[extensionlen])) {
			extensionlen++;
		}
	}

#undef dsep

	*ext = extension;
	return extensionlen;
}
