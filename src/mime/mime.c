/* Functionality for handling mime types */
/* $Id: mime.c,v 1.11 2003/06/08 18:47:01 jonas Exp $ */

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

unsigned char *
get_content_type(unsigned char *head, unsigned char *url)
{
	unsigned char *content_type;
	unsigned char *extension;

	/* If there's one in header, it's simple.. */
	if (head) {
	       	content_type = parse_http_header(head, "Content-Type", NULL);

		if (content_type) {
			unsigned char *s;
			int slen;

			s = strchr(content_type, ';');
			if (s) *s = '\0';

			slen = strlen(content_type);
			while (slen && content_type[--slen] <= ' ') {
				content_type[slen] = '\0';
			}

			return content_type;
		}
	}

	/* We can't use the extension string we are getting below, because we
	 * want to support also things like "ps.gz" - that'd never work, as we
	 * would always compare only to "gz". */
	/* Guess type accordingly to the extension */
	extension = get_extensionpart_from_url(url);
	if (extension) {
		int extlen;

		content_type = get_content_type_backends(extension);
		if (content_type) {
			mem_free(extension);
			return content_type;
		}

		extlen = strlen(extension);
		content_type = mem_alloc(15 + extlen);
		if (content_type) {
			/* Try to make application/x-<extension> from it */
			memcpy(content_type, "application/x-", 14);
			safe_strncpy(content_type + 14, extension, extlen + 1);

			if (get_mime_type_handler(NULL, content_type)) {
				mem_free(extension);
				return content_type;
			}

			mem_free(content_type);
		}

		mem_free(extension);
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
