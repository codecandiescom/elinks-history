/* Functionality for handling mime types */
/* $Id: mime.c,v 1.3 2003/05/16 22:29:52 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "terminal/terminal.h"
#include "mime/backend/common.h"
#include "mime/mime.h"
#include "osdep/os_dep.h"		/* For get_system_str() */
#include "protocol/http/header.h"	/* For parse_http_header() */
#include "protocol/url.h"
#include "util/memory.h"
#include "util/string.h"

void
init_mime()
{
	init_mime_backends();
}

void
done_mime()
{
	done_mime_backends();
}

unsigned char *
get_content_type(unsigned char *head, unsigned char *url)
{
	unsigned char *pos;
	unsigned char *extension;
	unsigned char *content_type;

	/* If there's one in header, it's simple.. */

	if (head) {
	       	content_type = parse_http_header(head, "Content-Type", NULL);

		if (content_type) {
			unsigned char *s;
			int slen;

			s = strchr(content_type, ';');
			if (s) *s = '\0';

			slen = strlen(content_type);
			while (slen && content_type[--slen - 1] <= ' ') {
				content_type[--slen] = '\0';
			}

			return content_type;
		}
	}

	/* We can't use the extension string we are getting below, because we
	 * want to support also things like "ps.gz" - that'd never work, as we
	 * would always compare only to "gz". */
	/* Guess type accordingly to the extension */
	content_type = get_content_type_backends(url);
	if (content_type) return content_type;

	/* Get extension */

	extension = NULL;

	/* Hmmm, well, can we do better there ? --Zas */
	for (pos = url; *pos && !end_of_dir(*pos); pos++) {
		if (*pos == '.') {
			extension = pos + 1;
		} else if (dir_sep(*pos)) {
			extension = NULL;
		}
	}

	if (extension) {
		unsigned char *ext_type = init_str();
		int el = 0;
		int ext_len = 0;

		if (!ext_type) return NULL; /* Bad thing. */

		while (extension[ext_len]
		       && !dir_sep(extension[ext_len])
		       && !end_of_dir(extension[ext_len])) {
			ext_len++;
		}

		/* Try to make application/x-extension from it */

		add_to_str(&ext_type, &el, "application/x-");
		add_bytes_to_str(&ext_type, &el, extension, ext_len);

		if (get_mime_type_handler(NULL, ext_type))
			return ext_type;

		mem_free(ext_type);
	}

	/* Fallback.. use some hardwired default */
	return stracpy(get_opt_str("mime.default_type"));
}

/* Find program to handle mimetype. The @term is for getting info about X
 * capabilities. */
struct mime_handler *
get_mime_type_handler(struct terminal *term, unsigned char *type)
{
	int have_x = term ? term->environment & ENV_XWIN : 0;

	return get_mime_handler_backends(type, have_x);
}
