/* MIME handling backends multiplexing */
/* $Id: common.c,v 1.8 2003/06/07 23:42:31 jonas Exp $ */

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
#include "mime/backend/mimetypes.h"

static INIT_LIST_HEAD(mime_backends);

void
init_mime_backends(void)
{
	struct mime_backend *backend;

	add_to_list(mime_backends, &default_mime_backend);
#ifdef MAILCAP
	add_to_list_bottom(mime_backends, &mailcap_mime_backend);
#endif
#ifdef MIMETYPES
	add_to_list_bottom(mime_backends, &mimetypes_mime_backend);
#endif

	foreach (backend, mime_backends)
		if (backend->init)
			backend->init();
}

void
done_mime_backends(void)
{
	struct mime_backend *backend;

	foreach (backend, mime_backends)
		if (backend->done)
			backend->done();
}

/* TODO Make backend selection scheme configurable */

unsigned char *
get_content_type_backends(unsigned char *uri)
{
	struct mime_backend *backend;

	foreach (backend, mime_backends)
		if (backend->get_content_type) {
			unsigned char *content_type;

			content_type = backend->get_content_type(uri);
			if (content_type) return content_type;
		}

	return NULL;
}

struct mime_handler *
get_mime_handler_backends(unsigned char *ctype, int have_x)
{
	struct mime_backend *backend;

	foreach (backend, mime_backends)
		if (backend->get_mime_handler) {
			struct mime_handler *handler;

			handler = backend->get_mime_handler(ctype, have_x);
			if (handler) return handler;
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
