/* Functionality for handling mime types */
/* $Id: mime.c,v 1.17 2003/06/18 00:34:06 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "mime/backend/common.h"
#include "mime/mime.h"
#include "protocol/http/header.h"	/* For parse_http_header() */
#include "protocol/url.h"
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

/* Trim the extension so only last .<extension> is used. */
static inline unsigned char *
try_extension_type(unsigned char *extension)
{
	struct mime_handler *handler;
	unsigned char *content_type;
	unsigned char *trimmed = strrchr(extension, '.');

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
	extension = get_extensionpart_from_url(url);
	if (extension) {
		unsigned char *ctype = get_content_type_backends(extension);

		/* Check if application/x-<extension> has any handlers. */
		if (!ctype)
			ctype = try_extension_type(extension);

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
