/* Functionality for handling mime types */
/* $Id: mime.c,v 1.13 2003/06/11 03:01:56 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "terminal/terminal.h"
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
	unsigned char *trimmed = strrchr(extension, '.');
	unsigned char *content_type;
	int trimmedlen;

	if (!trimmed)
		return NULL;

	trimmedlen = strlen(trimmed) + 1;
	content_type = mem_alloc(14 + trimmedlen);
	if (!content_type)
		return NULL;

	memcpy(content_type, "application/x-", 14);
	safe_strncpy(content_type + 14, trimmed, trimmedlen);

	handler = get_mime_type_handler(NULL, content_type);
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
get_mime_type_handler(struct terminal *term, unsigned char *content_type)
{
	int have_x = term ? term->environment & ENV_XWIN : 0;

	return get_mime_handler_backends(content_type, have_x);
}
