/* Support for mime.types files for mapping file extensions to content types */
/* $Id: mimetypes.c,v 1.16 2003/10/03 12:35:23 jonas Exp $ */

/* Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 2003-	   The ELinks Project */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef MIMETYPES

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"
#include "setup.h"

#include "config/options.h"
#include "mime/backend/common.h"
#include "mime/backend/mimetypes.h"
#include "mime/mime.h"
#include "sched/session.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"

#define BACKEND_NAME	"mimetypes"

struct mimetypes_entry {
	unsigned char *content_type;
	unsigned char extension[1];
};

/* State variables */
static struct hash *mimetypes_map = NULL;
static int mimetypes_map_size = 0;
static struct option *mimetypes_tree = NULL;


static void
done_mimetypes_entry(struct mimetypes_entry *entry)
{
	if (!entry) return;
	if (entry->content_type) mem_free(entry->content_type);
	mem_free(entry);
}

/* Parsing of a mime.types file with the format:
 *
 *	basetype/subtype	extension1 [extension2 ... extensionN]
 *
 * Comments starts with '#'. */

#define skip_whitespace(S) \
	do { while (*(S) && isspace(*(S))) (S)++; } while (0)

static inline void
parse_mimetypes_extensions(unsigned char *token, unsigned char *ctype)
{
	int ctypelen = strlen(ctype);

	/* Cycle through the file extensions */
	while (*token) {
		struct mimetypes_entry *entry;
		unsigned char *extension;
		struct hash_item *item;
		int extlen;

		skip_whitespace(token);

		extension = token;
		while (*token && !isspace(*token))
			token++;

		if (!*token) break;
		*token++ = '\0';

		extlen = strlen(extension);
		/* First check if the type is already known. If it is
		 * drop it. This way first files are priotized. */
		item = get_hash_item(mimetypes_map, extension, extlen);
		if (item) continue;

		entry = mem_calloc(1, sizeof(struct mimetypes_entry) + extlen);
		if (!entry) continue;

		entry->content_type = memacpy(ctype, ctypelen);
		if (!entry->content_type) {
			done_mimetypes_entry(entry);
			continue;
		}

		memcpy(entry->extension, extension, extlen);

		item = add_hash_item(mimetypes_map, entry->extension, extlen,
				     entry);

		if (item)
			mimetypes_map_size++;
		else
			done_mimetypes_entry(entry);
	}
}

static void
parse_mimetypes_file(unsigned char *filename)
{
	FILE *file = fopen(filename, "r");
	unsigned char line[MAX_STR_LEN];

	if (!file) return;

	while (fgets(line, MAX_STR_LEN - 1, file)) {
		unsigned char *ctype = line;
		unsigned char *token;

		/* Weed out any comments */
		token = strchr(line, '#');
		if (token)
			*token = '\0';

		skip_whitespace(ctype);

		/* Position on the next field in this line */
		token = ctype;
		while (*token && !isspace(*token))
			token++;

		if (!*token) continue;
		*token++ = '\0';

		/* Check if malformed content type */
		if (!strchr(ctype, '/')) continue;

		parse_mimetypes_extensions(token, ctype);
	}

	fclose(file);
}

#undef skip_whitespace

static void
init_mimetypes_map(void)
{
	unsigned char *path;

	if (!get_opt_bool_tree(mimetypes_tree, "enable"))
		return;

	mimetypes_map = init_hash(8, &strhash);
	if (!mimetypes_map)
		return;

	/* Determine the path  */
	path = get_opt_str_tree(mimetypes_tree, "path");
	if (!path || !*path)
		path = DEFAULT_MIMETYPES_PATH;

	while (*path) {
		unsigned char *filename = get_next_path_filename(&path, ':');

		if (!filename) continue;
		parse_mimetypes_file(filename);
		mem_free(filename);
	}
}

static void
done_mimetypes(void)
{
	struct hash_item *item;
	int i;

	if (!mimetypes_map)
		return;

	foreach_hash_item (item, *mimetypes_map, i)
		if (item->value) {
			struct mimetypes_entry *entry = item->value;

			done_mimetypes_entry(entry);
		}

	free_hash(mimetypes_map);
	mimetypes_map = NULL;
	mimetypes_map_size = 0;
}

static int
mimetypes_change_hook(struct session *ses, struct option *current,
		      struct option *changed)
{
	if (!strncasecmp(changed->name, "path", 4)) {
		/* Brute forcing reload! */
		done_mimetypes();
		init_mimetypes_map();
	} else if (!strncasecmp(changed->name, "enable", 6)) {
		int enable = *((int *) changed->ptr);

		if (enable && !mimetypes_map)
			init_mimetypes_map();
		else if (!enable && mimetypes_map)
			done_mimetypes();
	}

	return 0;
}

static void
init_mimetypes(void)
{
	mimetypes_tree = get_opt_rec(config_options, "mime.mimetypes");
	mimetypes_tree->change_hook = mimetypes_change_hook;
	init_mimetypes_map();
}


static unsigned char *
get_content_type_mimetypes(unsigned char *extension)
{
	struct hash_item *item;
	int extensionlen;

	if (!mimetypes_map)
		return NULL;

	extensionlen = strlen(extension);
	while (1) {
		unsigned char *trimmed;

		/* First the given type is looked up. */
		item = get_hash_item(mimetypes_map, extension, extensionlen);

		/* Check list of entries */
		if (item && item->value) {
			struct mimetypes_entry *entry = item->value;

			return stracpy(entry->content_type);
		}

		/* Try to trim the extension from the left. */
		trimmed = strchr(extension, '.');
		if (!trimmed)
			break;

		extensionlen -= trimmed - extension + 1;
		extension = trimmed + 1;
	}

	return NULL;
}

/* Setup the exported backend */
struct mime_backend mimetypes_mime_backend = {
	NULL_LIST_HEAD,
	/* name: */		BACKEND_NAME,
	/* init: */		init_mimetypes,
	/* done: */		done_mimetypes,
	/* get_content_type: */	get_content_type_mimetypes,
	/* get_mime_handler: */	NULL,
};

#endif /* MIMETYPES */
