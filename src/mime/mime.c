/* Functionality for handling mime types */
/* $Id: mime.c,v 1.9 2003/06/07 22:52:36 jonas Exp $ */

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
	int extensionlen;

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
	content_type = get_content_type_backends(url);
	if (content_type)
		return content_type;

	extensionlen = get_extension_from_url(url, &extension);

	if (extensionlen) {
		unsigned char *ext_type = mem_alloc(15 + extensionlen);

		if (!ext_type) return NULL;

		/* Try to make application/x-<extension> from it */
		memcpy(ext_type, "application/x-", 14);
		safe_strncpy(ext_type + 14, extension, extensionlen + 1);

		if (get_mime_type_handler(NULL, ext_type))
			return ext_type;

		mem_free(ext_type);
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
