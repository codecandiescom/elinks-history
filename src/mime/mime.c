/* Functionality for handling mime types */
/* $Id: mime.c,v 1.47 2004/04/24 16:03:23 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "encoding/encoding.h"
#include "intl/gettext/libintl.h"
#include "mime/backend/common.h"
#include "mime/mime.h"
#include "modules/module.h"
#include "protocol/http/header.h"	/* For parse_http_header() */
#include "protocol/uri.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"


enum mime_options {
	MIME_TREE,
	MIME_DEFAULT_TYPE,

	MIME_OPTIONS,
};

static struct option_info mime_options[] = {
	INIT_OPT_TREE("", N_("MIME"),
		"mime", OPT_SORT,
		N_("MIME-related options (handlers of various MIME types).")),

	INIT_OPT_STRING("mime", N_("Default MIME-type"),
		"default_type", 0, DEFAULT_MIME_TYPE,
		N_("Document MIME-type to assume by default (when we are unable to\n"
		"guess it properly from known information about the document).")),

	NULL_OPTION_INFO,
};

#define get_opt_mime(which)	mime_options[(which)].option
#define get_default_mime_type()	get_opt_mime(MIME_DEFAULT_TYPE).value.string

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

	for (extension_list = listext_encoded(encoding);
	     extension_list && *extension_list;
	     extension_list++) {
		unsigned char *content_type;

		if (strcmp(*extension_list, last_extension))
			continue;

		*last_extension = '\0';
		content_type = get_content_type_backends(extension);
		*last_extension = '.';

		return content_type;
	}

	return NULL;
}

#if 0
#define DEBUG_CONTENT_TYPE
#endif

#ifdef DEBUG_CONTENT_TYPE
#define debug_get_content_type_params(head__, url__) \
	DBG("get_content_type(head, url)\n=== head ===\n%s\n=== url ===\n%s\n", head__, struri(url__))
#define debug_ctype(ctype__) DBG("ctype= %s", (ctype__))
#define debug_extension(extension__) DBG("extension= %s", (extension__))
#else
#define debug_get_content_type_params(head__, url__)
#define debug_ctype(ctype__)
#define debug_extension(extension__)
#endif

unsigned char *
get_content_type(struct cache_entry *cached)
{
	struct uri *uri = get_cache_uri(cached);
	unsigned char *extension, *ctype;

	debug_get_content_type_params(head, uri);

	/* If there's one in header, it's simple.. */
	if (cached->head) {
		ctype = parse_http_header(cached->head, "Content-Type", NULL);
		if (ctype) {
			unsigned char *end = strchr(ctype, ';');
			int ctypelen;

			if (end) *end = '\0';

			ctypelen = strlen(ctype);
			while (ctypelen && ctype[--ctypelen] <= ' ')
				ctype[ctypelen] = '\0';

			debug_ctype(ctype);

			if (*ctype) return ctype;
			mem_free(ctype);
		}
	}

	/* We can't use the extension string we are getting below, because we
	 * want to support also things like "ps.gz" - that'd never work, as we
	 * would always compare only to "gz". */
	/* Guess type accordingly to the extension */
	extension = uri ? get_extension_from_uri(uri) : NULL;
	debug_extension(extension);
	if (extension) {

		ctype = get_content_type_backends(extension);
		debug_ctype(ctype);
		if (!ctype) ctype = check_encoding_type(extension);
		debug_ctype(ctype);
		if (!ctype) ctype = check_extension_type(extension);
		debug_ctype(ctype);

		mem_free(extension);

		if (ctype) return ctype;
	}

	ctype = get_default_mime_type();
	debug_ctype(ctype);

	/* Fallback.. use some hardwired default */
	return stracpy(ctype);
}

struct mime_handler *
get_mime_type_handler(unsigned char *content_type, int xwin)
{
	return get_mime_handler_backends(content_type, xwin);
}

unsigned char *
get_content_filename(struct uri *uri)
{
	struct cache_entry *cached = find_in_cache(uri);
	unsigned char *filename, *pos;

	if (!cached || !cached->head)
		return NULL;

	pos = parse_http_header(cached->head, "Content-Disposition", NULL);
	if (!pos) return NULL;

	filename = parse_http_header_param(pos, "filename");
	mem_free(pos);
	if (!filename) return NULL;

	/* We don't want to add any directories from the path so make sure we
	 * only add the filename. */
	pos = get_filename_position(filename);
	if (!*pos) {
		mem_free(filename);
		return NULL;
	}

	if (pos > filename)
		memmove(filename, pos, strlen(pos) + 1);

	return filename;
}

/* Backends dynamic area: */

#include "mime/backend/default.h"
#include "mime/backend/mailcap.h"
#include "mime/backend/mimetypes.h"

static struct module *mime_submodules[] = {
	&default_mime_module,
#ifdef CONFIG_MAILCAP
	&mailcap_mime_module,
#endif
#ifdef CONFIG_MIMETYPES
	&mimetypes_mime_module,
#endif
	NULL,
};

struct module mime_module = struct_module(
	/* name: */		N_("MIME"),
	/* options: */		mime_options,
	/* hooks: */		NULL,
	/* submodules: */	mime_submodules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
