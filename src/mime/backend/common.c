/* MIME handling backends multiplexing */
/* $Id: common.c,v 1.15 2003/10/20 14:54:55 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "mime/backend/common.h"
#include "mime/mime.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"


/* Backends dynamic area: */

#include "mime/backend/default.h"
#include "mime/backend/mailcap.h"
#include "mime/backend/mimetypes.h"

static struct mime_backend *mime_backends[] = {
	&default_mime_backend,
#ifdef MAILCAP
	&mailcap_mime_backend,
#endif
#ifdef MIMETYPES
	&mimetypes_mime_backend,
#endif
	NULL,

};

void
init_mime_backends(void)
{
	struct mime_backend **backend;

	for (backend = mime_backends; *backend; backend++)
		if ((*backend)->init)
			(*backend)->init();
}

void
done_mime_backends(void)
{
	struct mime_backend **backend;

	for (backend = mime_backends; *backend; backend++)
		if ((*backend)->done)
			(*backend)->done();
}

unsigned char *
get_content_type_backends(unsigned char *extension)
{
	struct mime_backend **backend;

	for (backend = mime_backends; *backend; backend++)
		if ((*backend)->get_content_type) {
			unsigned char *content_type;

			content_type = (*backend)->get_content_type(extension);
			if (content_type) return content_type;
		}

	return NULL;
}

struct mime_handler *
get_mime_handler_backends(unsigned char *ctype, int have_x)
{
	struct mime_backend **backend;

	for (backend = mime_backends; *backend; backend++)
		if ((*backend)->get_mime_handler) {
			struct mime_handler *handler;

			handler = (*backend)->get_mime_handler(ctype, have_x);
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
