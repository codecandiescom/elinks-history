/* Functionality for handling mime types */
/* $Id: mime.c,v 1.24 2003/07/15 21:33:18 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "mime/backend/common.h"
#include "mime/mime.h"
#include "protocol/http/header.h"	/* For parse_http_header() */
#include "protocol/uri.h"
#include "util/encoding.h"
#include "util/memory.h"
#include "util/string.h"

void
init_mime(void)
{
	init_mime_backends();
}

void
done_mime(void)
{
	done_mime_backends();
}

/* Checks if application/x-<extension> has any handlers. */
static inline unsigned char *
check_extension_type(unsigned char *extension)
{
	/* Trim the extension so only last .<extension> is used. */
	unsigned char *trimmed = strrchr(extension, '.');
	struct mime_handler *handler;
	unsigned char *content_type;

	if (!trimmed)
		return NULL;

	content_type = straconcat("application/x-", trimmed + 1, NULL);
	if (!content_type)
		return NULL;

	handler = get_mime_type_handler(content_type, 1);
	if (handler) {
		mem_free(handler->program);
		mem_free(handler);
		return content_type;
	}

	mem_free(content_type);
	return NULL;
}

/* Check if part of the extension coresponds to a supported encoding and if it
 * has any handlers. */
static inline unsigned char *
check_encoding_type(unsigned char *extension)
{
	enum stream_encoding encoding = guess_encoding(extension);
	unsigned char **extension_list;
	unsigned char *last_extension = strrchr(extension, '.');

	if (encoding == ENCODING_NONE || !last_extension)
		return NULL;

	extension_list = listext_encoded(encoding);

	for (; extension_list && *extension_list; extension_list++) {
		if (!strcmp(*extension_list, last_extension)) {
			unsigned char *content_type;

			*last_extension = '\0';
			content_type = get_content_type_backends(extension);
			*last_extension = '.';

			return content_type;
		}
	}

	return NULL;
}

unsigned char *
get_content_type(unsigned char *head, unsigned char *url)
{
	unsigned char *extension;

	/* If there's one in header, it's simple.. */
	if (head) {
		unsigned char *ctype;

		ctype = parse_http_header(head, "Content-Type", NULL);
		if (ctype) {
			unsigned char *end = strchr(ctype, ';');
			int ctypelen;

			if (end) *end = '\0';

			ctypelen = strlen(ctype);
			while (ctypelen && ctype[--ctypelen] <= ' ')
				ctype[ctypelen] = '\0';

			return ctype;
		}
	}

	/* We can't use the extension string we are getting below, because we
	 * want to support also things like "ps.gz" - that'd never work, as we
	 * would always compare only to "gz". */
	/* Guess type accordingly to the extension */
	extension = get_extension_from_url(url);
	if (extension) {
		unsigned char *ctype = get_content_type_backends(extension);

		if (!ctype) ctype = check_encoding_type(extension);
		if (!ctype) ctype = check_extension_type(extension);

		mem_free(extension);

		if (ctype)
			return ctype;
	}

	/* Fallback.. use some hardwired default */
	return stracpy(get_opt_str("mime.default_type"));
}

struct mime_handler *
get_mime_type_handler(unsigned char *content_type, int xwin)
{
	return get_mime_handler_backends(content_type, xwin);
}
