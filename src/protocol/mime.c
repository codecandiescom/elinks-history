/* Internal MIME types implementation */
/* $Id: mime.c,v 1.6 2002/12/01 17:28:53 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "config/options.h"
#include "lowlevel/terminal.h"
#include "protocol/http/header.h"
#include "protocol/mime.h"
#include "protocol/url.h"
#include "util/memory.h"
#include "util/string.h"


/* Guess content type of the document. */
unsigned char *
get_content_type(unsigned char *head, unsigned char *url)
{
	unsigned char *pos, *extension;
	int ext_len;

	/* If there's one in header, it's simple.. */

	if (head) {
	       	char *ctype = parse_http_header(head, "Content-Type", NULL);

		if (ctype) {
			unsigned char *s;
			int slen;

			s = strchr(ctype, ';');
			if (s) *s = '\0';

			slen = strlen(ctype);
			while (slen && ctype[slen - 1] <= ' ') {
				ctype[--slen] = '\0';
			}

			return ctype;
		}
	}

	/* We can't use the extension string we are getting below, because we
	 * want to support also things like "ps.gz" - that'd never work, as we
	 * would always compare only to "gz". */

	/* Guess type accordingly to the extension */

	{
		struct option *opt_tree = get_opt_rec_real(root_options,
							   "mime.extension");
		struct option *opt;

		foreach (opt, *((struct list_head *) opt_tree->ptr)) {
			/* strrcmp */
			int i, j;

			/* Match the longest possible part of URL.. */

			for (i = strlen(url) - 1, j = strlen(opt->name) - 1;
			     i >= 0 && j >= 0
			     && url[i] == (opt->name[j] == '*' ? '.'
				     			       : opt->name[j]);
			     i--, j--)
				/* */ ;

			/* If we matched whole extension and it is really an
			 * extension.. */
			if (j < 0 && i >= 0 && url[i] == '.') {
				return stracpy(opt->ptr);
			}
		}
	}

	/* Get extension */

	extension = NULL;
	ext_len = 0;

	for (pos = url; *pos && !end_of_dir(*pos); pos++) {
		if (*pos == '.') {
			extension = pos + 1;
		} else if (dir_sep(*pos)) {
			extension = NULL;
		}
	}

	if (extension) {
		while (extension[ext_len]
		       && !dir_sep(extension[ext_len])
		       && !end_of_dir(extension[ext_len])) {
			ext_len++;
		}
	}

	/* Try to make application/x-extension from it */

	if (extension) {
		unsigned char *ext_type = init_str();
		int el = 0;

		if (ext_type) {
			add_to_str(&ext_type, &el, "application/x-");
			add_bytes_to_str(&ext_type, &el, extension, ext_len);

			if (get_mime_type_handler(NULL, ext_type))
				return ext_type;

			mem_free(ext_type);
		}
	}

	/* Fallback.. use some hardwired default */
	/* TODO: Make this rather mime.extension._template_ ..? --pasky */

	return stracpy(get_opt_str("document.download.default_mime_type"));
}



void
rmdots(unsigned char *tok)
{
	while (*tok) {
		if (*tok == '.') *tok = '*';
		tok++;
	}
}

unsigned char *
get_mime_type_name(unsigned char *type)
{
	unsigned char *class, *id;
	unsigned char *name;

	class = stracpy(type);
	if (!class) return NULL;
	rmdots(class);

	id = strchr(class, '/');
	if (!id) {
		mem_free(class);
		return NULL;
	}
	*(id++) = '\0';
	rmdots(id);

	name = straconcat("mime.type.", class, ".", id, NULL);
	mem_free(class);

	return name;
}

unsigned char *
get_mime_handler_name(unsigned char *type, int xwin)
{
	struct option *opt;
	unsigned char *name = get_mime_type_name(type);
	unsigned char *system_str;

	if (!name) return NULL;

	opt = get_opt_rec_real(root_options, name);
	mem_free(name);
	if (!opt) return NULL;

	system_str = get_system_str(xwin);
	if (!system_str) return NULL;

	name = straconcat("mime.handler.", (unsigned char *) opt->ptr,
			  ".", system_str, NULL);
	mem_free(system_str);

	return name;
}

/* Return tree containing options specific to this type. */
struct option *
get_mime_type_handler(struct terminal *term, unsigned char *type)
{
	struct option *opt_tree;
	unsigned char *name;
	int xwin = term ? term->environment & ENV_XWIN : 0;

	name = get_mime_handler_name(type, xwin);
	if (!name) return NULL;

	opt_tree = get_opt_rec_real(root_options, name);

	mem_free(name);

	return opt_tree;
}


/* TODO: Move this *AWAY*! To protocol/user.c, if possible ;-). --pasky */
unsigned char *
get_prog(struct terminal *term, unsigned char *progid)
{
	struct option *opt;
	unsigned char *system_str =
		get_system_str(term ? term->environment & ENV_XWIN : 0);
	unsigned char *name;

	if (!system_str) return NULL;
	name = straconcat("protocol.user.", progid, ".",
			  system_str, NULL);
	mem_free(system_str);
	if (!name) return NULL;

	opt = get_opt_rec_real(root_options, name);

	mem_free(name);
	return (unsigned char *) (opt ? opt->ptr : NULL);
}
