/* RFC1524 (mailcap file) implementation */
/* $Id: mailcap.c,v 1.42 2003/06/22 23:17:21 jonas Exp $ */

/* This file contains various functions for implementing a fair subset of
 * rfc1524.
 *
 * The rfc1524 defines a format for the Multimedia Mail Configuration, which is
 * the standard mailcap file format under Unix which specifies what external
 * programs should be used to view/compose/edit multimedia files based on
 * content type.
 *
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (c) 2002-2003 The ELinks project
 *
 * This file was hijacked from the Mutt project <URL:http://www.mutt.org>
 * (version 1.4) on Saturday the 7th December 2002. It has been heavily
 * elinksified. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef MAILCAP

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "mime/backend/common.h"
#include "mime/backend/mailcap.h"
#include "mime/mime.h"
#include "osdep/os_dep.h"		/* For exe() */
#include "sched/session.h"
#include "util/file.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"

#define BACKEND_NAME	"mailcap"

struct mailcap_hash_item {
	/* The entries associated with the type */
	struct list_head entries; /* -> struct mailcap_entry */

	/* The content type of all @entries. Must be last! */
	unsigned char type[1];
};

struct mailcap_entry {
	LIST_HEAD(struct mailcap_entry);

	/* The 'raw' unformatted (view)command from the mailcap files. */
	unsigned char *command;

	/* To verify if command qualifies. Cannot contain %s formats. */
	unsigned char *testcommand;

	/* Used to inform the user of the type or handler. */
	unsigned char *description;

	/* Used to determine between an exact match and a wildtype match. Lower
	 * is better. Increased for each sourced file. */
	unsigned int priority;

	/* Whether the program "blocks" the term. */
	unsigned int needsterminal:1;

	/* If "| ${PAGER}" should be added. It would of course be better to
	 * pipe the output into a buffer and let ELinks display it but this
	 * will have to do for now. */
	unsigned int copiousoutput:1;
};

/* State variables */
static struct hash *mailcap_map = NULL;
static int mailcap_map_size = 0;
static struct option *mailcap_tree = NULL;


static inline void
done_mailcap_entry(struct mailcap_entry *entry)
{
	if (!entry) return;
	if (entry->testcommand)	mem_free(entry->testcommand);
	if (entry->description)	mem_free(entry->description);
	mem_free(entry);
}

/* Takes care of all initialization of mailcap entries.
 * Clear memory to make freeing it safer later and we get
 * needsterminal and copiousoutput initialized for free. */
static inline struct mailcap_entry *
init_mailcap_entry(unsigned char *command, int priority)
{
	struct mailcap_entry *entry;
	int commandlen = strlen(command);

	entry = mem_calloc(1, sizeof(struct mailcap_entry) + commandlen + 1);
	if (!entry)
		return NULL;

	entry->command = (unsigned char *)entry + sizeof(struct mailcap_entry);
	safe_strncpy(entry->command, command, commandlen + 1);

	entry->priority = priority;

	return entry;
}

static inline void
add_mailcap_entry(struct mailcap_entry *entry, unsigned char *type, int typelen)
{
	struct mailcap_hash_item *mitem;
	struct hash_item *item;

	/* Time to get the entry into the mailcap_map */
	/* First check if the type is already checked in */
	item = get_hash_item(mailcap_map, type, typelen);
	if (!item) {
		mitem = mem_alloc(sizeof(struct mailcap_hash_item) + typelen);
		if (!mitem) {
			done_mailcap_entry(entry);
			return;
		}

		safe_strncpy(mitem->type, type, typelen + 1);

		init_list(mitem->entries);

		item = add_hash_item(mailcap_map, mitem->type, typelen, mitem);
		if (!item) {
			mem_free(mitem);
			done_mailcap_entry(entry);
			return;
		}
	} else if (!item->value) {
		done_mailcap_entry(entry);
		return;
	} else {
		mitem = item->value;
	}

	add_to_list_bottom(mitem->entries, entry);
	mailcap_map_size++;
}

/* Parsing of a RFC1524 mailcap file */
/* The format is:
 *
 *	base/type; command; extradefs
 *
 * type can be * for matching all; base with no /type is an implicit
 * wildcard; command contains a %s for the filename to pass, default to pipe on
 * stdin; extradefs are of the form:
 *
 *	def1="definition"; def2="define \;";
 *
 * line wraps with a \ at the end of the line, # for comments. */
/* TODO handle default pipe. Maybe by prepending "cat |" to the command. */

#define skip_whitespace(S) \
	do { while (*(S) && isspace(*(S))) (S)++; } while (0)

/* Returns a NULL terminated RFC 1524 field, while modifying @next to point
 * to the next field. */
static unsigned char *
get_mailcap_field(unsigned char **next)
{
	unsigned char *field;
	unsigned char *fieldend;

	if (!next || !*next) return NULL;

	field = *next;
	skip_whitespace(field);
	fieldend = field;

	/* End field at the next occurence of ';' but not escaped '\;' */
	do {
		/* Handle both if ';' is the first char or if it's escaped */
		if (*fieldend == ';')
			fieldend++;

		fieldend = strchr(fieldend, ';');
	} while (fieldend && *(fieldend-1) == '\\');

	if (fieldend) {
		*fieldend = '\0';
		*next = fieldend;
		fieldend--;
		(*next)++;
		skip_whitespace(*next);
	} else {
		*next = NULL;
		fieldend = field + strlen(field) - 1;
	}

	/* Remove trailing whitespace */
	while (field <= fieldend && isspace(*fieldend))
		*fieldend-- = '\0';

	return field;
}

/* Parses specific fields (ex: the '=TestCommand' part of 'test=TestCommand').
 * Expects that @field is pointing right after the specifier (ex: 'test'
 * above). Allocates and returns a NULL terminated token, or NULL if parsing
 * fails. */

static unsigned char *
get_mailcap_field_text(unsigned char *field)
{
	skip_whitespace(field);

	if (*field == '=') {
		field++;
		skip_whitespace(field);

		return stracpy(field);
	}

	return NULL;
}

#undef skip_whitespace

/* Parse optional extra definitions. Zero return value means syntax error  */
static inline int
parse_optional_fields(struct mailcap_entry *entry, unsigned char *line)
{
	while (0xf131d5) {
		unsigned char *field = get_mailcap_field(&line);

		if (!field) break;

		if (!strncasecmp(field, "needsterminal", 13)) {
				entry->needsterminal = 1;

		} else if (!strncasecmp(field, "copiousoutput", 13)) {
			entry->copiousoutput = 1;

		} else if (!strncasecmp(field, "test", 4)) {
			entry->testcommand = get_mailcap_field_text(field + 4);
			if (!entry->testcommand)
				return 0;

			/* Find out wether testing requires filename */
			for (field = entry->testcommand; *field; field++)
				if (*field == '%' && *(field+1) == 's') {
					mem_free(entry->testcommand);
					return 0;
				}

		} else if (!strncasecmp(field, "description", 11)) {
			entry->description = get_mailcap_field_text(field + 11);
			if (!entry->description)
				return 0;
		}
	}

	return 1;
}

/* Parses whole mailcap files line-by-line adding entries to the map
 * assigning them the given @priority */
static void
parse_mailcap_file(unsigned char *filename, unsigned int priority)
{
	FILE *file = fopen(filename, "r");
	unsigned char *line = NULL;
	size_t linelen = MAX_STR_LEN;
	int lineno = 1;

	if (!file) return;

	while ((line = file_read_line(line, &linelen, file, &lineno))) {
		struct mailcap_entry *entry;
		unsigned char *linepos;
		unsigned char *command;
		unsigned char *basetypeend;
		unsigned char *type;
		int typelen;


		/* Ignore comments */
		if (*line == '#') continue;

		linepos = line;

		/* Get type */
		type = get_mailcap_field(&linepos);
		if (!type) continue;

		/* Next field is the viewcommand */
		command = get_mailcap_field(&linepos);
		if (!command) continue;

		entry = init_mailcap_entry(command, priority);
		if (!entry) continue;

		if (!parse_optional_fields(entry, linepos)) {
			done_mailcap_entry(entry);
			error("Bad formated entry for type %s in \"%s\" line %d",
			      type, filename, lineno);
			continue;
		}

		basetypeend = strchr(type, '/');
		typelen = strlen(type);

		if (!basetypeend) {
			unsigned char implicitwild[64];

			if (typelen + 3 > 64) {
				done_mailcap_entry(entry);
				continue;
			}

			memcpy(implicitwild, type, typelen);
			implicitwild[typelen++] = '/';
			implicitwild[typelen++] = '*';
			implicitwild[typelen++] = '\0';
			add_mailcap_entry(entry, implicitwild, typelen);
			continue;
		}

		add_mailcap_entry(entry, type, typelen);
	}

	fclose(file);
	if (line) mem_free(line); /* Alloced by file_read_line() */
}


/* When initializing mailcap subsystem we read, parse and build a hash mapping
 * content type to handlers. Map is built from a list of mailcap files.
 *
 * The RFC1524 specifies that a path of mailcap files should be used.
 *	o First we check to see if the user supplied any in mime.mailcap.path
 *	o Then we check the MAILCAP environment variable.
 *	o Finally fall back to reasonable default
 */

static int
mailcap_change_hook(struct session *, struct option *, struct option *);

static void
init_mailcap(void)
{
	unsigned char *path;
	unsigned int priority = 0;

	/* Check and do stuff that should only be done once. */
	if (!mailcap_tree) {
		mailcap_tree = get_opt_rec(&root_options, "mime.mailcap");
		mailcap_tree->change_hook = mailcap_change_hook;

		if (!get_opt_bool_tree(mailcap_tree, "enable"))
			return;
	}

	mailcap_map = init_hash(8, &strhash);
	if (!mailcap_map)
		return;

	/* Try to setup mailcap_path */
	path = get_opt_str_tree(mailcap_tree, "path");
	if (!path || !*path) path = getenv("MAILCAP");
	if (!path) path = DEFAULT_MAILCAP_PATH;

	while (*path) {
		unsigned char *filename = get_next_path_filename(&path, ':');

		if (!filename) continue;
		parse_mailcap_file(filename, priority++);
		mem_free(filename);
	}
}

static void
done_mailcap(void)
{
	struct hash_item *item;
	int i;

	if (!mailcap_map)
		return;

	foreach_hash_item (item, *mailcap_map, i) {
		struct mailcap_hash_item *mitem = item->value;

		if (!mitem) continue;

		while (!list_empty(mitem->entries)) {
			struct mailcap_entry *entry = mitem->entries.next;

			del_from_list(entry);
			done_mailcap_entry(entry);
		}

		mem_free(mitem);
	}

	free_hash(mailcap_map);
	mailcap_map = NULL;
	mailcap_map_size = 0;
}

static int
mailcap_change_hook(struct session *ses, struct option *current,
		    struct option *changed)
{
	if (!strncasecmp(changed->name, "path", 4)) {
		/* Brute forcing reload! */
		done_mailcap();
		init_mailcap();
	} else if (!strncasecmp(changed->name, "enable", 6)) {
		int enable = *((int *) changed->ptr);

		if (enable && !mailcap_map)
			init_mailcap();
		else if (!enable && mailcap_map)
			done_mailcap();
	}

	return 0;
}


/* The command semantics include the following:
 *
 * %s		is the filename that contains the mail body data
 * %t		is the content type, like text/plain
 * %{parameter} is replaced by the parameter value from the content-type
 *		field
 * \%		is %
 *
 * Unsupported RFC1524 parameters: these would probably require some doing
 * by Mutt, and can probably just be done by piping the message to metamail:
 *
 * %n		is the integer number of sub-parts in the multipart
 * %F		is "content-type filename" repeated for each sub-part
 * Only % is supported by subst_file() which is equivalent to %s. */

/* The formatting is postponed until the command is needed. This means
 * @type can be NULL. If '%t' is used in command we bail out. */
static unsigned char *
format_command(unsigned char *command, unsigned char *type, int copiousoutput)
{
	unsigned char *cmd = init_str();
	int cmdlen = 0;

	if (!cmd)
		return NULL;

	while (*command) {
		unsigned char *start = command;

		while (*command && *command != '%' && *command != '\\')
			command++;

		if (start < command)
			add_bytes_to_str(&cmd, &cmdlen, start, command - start);

		if (*command == '%') {
			command++;
			if (!*command) {
				mem_free(cmd);
				return NULL;

			} else if (*command == 's') {
				add_chr_to_str(&cmd, &cmdlen, '%');

			} else if (*command == 't') {
				if (!type) {
					mem_free(cmd);
					return NULL;
				}

				add_to_str(&cmd, &cmdlen, type);
			}
			command++;

		} else if (*command == '\\') {
			command++;
			if (*command) {
				add_chr_to_str(&cmd, &cmdlen, *command);
				command++;
			}
		}
	}

	if (copiousoutput) {
		unsigned char *pager = getenv("PAGER");

		if (!pager && file_exists(DEFAULT_PAGER_PATH)) {
			pager = DEFAULT_PAGER_PATH;
		} else if (!pager && file_exists(DEFAULT_LESS_PATH)) {
			pager = DEFAULT_LESS_PATH;
		} else if (!pager && file_exists(DEFAULT_MORE_PATH)) {
			pager = DEFAULT_MORE_PATH;
		}

		if (pager) {
			add_chr_to_str(&cmd, &cmdlen, '|');
			add_to_str(&cmd, &cmdlen, pager);
		}
	}

	return cmd;
}

/* Returns first usable mailcap_entry from a list where @entry is the head.
 * Use of @filename is not supported (yet). */
static struct mailcap_entry *
check_entries(struct mailcap_hash_item *item)
{
	struct mailcap_entry *entry;

	foreach (entry, item->entries) {
		unsigned char *test;

		/* Accept current if no test is needed */
		if (!entry->testcommand)
			return entry;

		/* We have to run the test command */
		test = format_command(entry->testcommand, NULL, 0);
		if (test) {
			int exitcode = exe(test);

			mem_free(test);
			if (!exitcode)
				return entry;
		}
	}

	return NULL;
}

/* Attempts to find the given type in the mailcap association map.  On success,
 * this returns the associated command, else NULL.  Type is a string with
 * syntax '<base>/<type>' (ex: 'text/plain')
 *
 * First the given type is looked up. Then the given <base>-type with added
 * wildcard '*' (ex: 'text/<star>'). For each lookup all the associated
 * entries are checked/tested.
 *
 * The lookup supports testing on files. If no file is given (NULL) any tests
 * that need a file will be taken as failed. */

static struct mime_handler *
get_mime_handler_mailcap(unsigned char *type, int options)
{
	struct mailcap_entry *entry;
	struct hash_item *item;

	if (!mailcap_map)
		return NULL;

	item = get_hash_item(mailcap_map, type, strlen(type));

	/* Check list of entries */
	entry = (item && item->value) ? check_entries(item->value) : NULL;

	if (!entry || get_opt_bool_tree(mailcap_tree, "prioritize")) {
		/* The type lookup has either failed or we need to check
		 * the priorities so get the wild card handler */
		struct mailcap_entry *wildcard = NULL;
		unsigned char *wildpos = strchr(type, '/');

		if (wildpos) {
			int wildlen = wildpos - type + 1; /* include '/' */
			unsigned char *wildtype = memacpy(type, wildlen + 2);

			if (!wildtype)
				return NULL;

			wildtype[wildlen++] = '*';
			wildtype[wildlen] = '\0';

			item = get_hash_item(mailcap_map, wildtype, wildlen);
			mem_free(wildtype);

			if (item && item->value)
				wildcard = check_entries(item->value);
		}

		/* Use @wildcard if its priority is better or @entry is NULL */
		if (wildcard && (!entry || (wildcard->priority < entry->priority)))
			entry = wildcard;
	}

	if (entry) {
		struct mime_handler *handler;
		unsigned char *program;

		program = format_command(entry->command, type,
					 entry->copiousoutput);
		if (!program) return NULL;

		handler = mem_alloc(sizeof(struct mime_handler));
		if (!handler) {
			mem_free(program);
			return NULL;
		}

		handler->block = (entry->needsterminal || entry->copiousoutput);
		handler->ask = get_opt_bool_tree(mailcap_tree, "ask");
		handler->program = program;
		handler->description = entry->description;
		handler->backend_name = BACKEND_NAME;

		return handler;
	}

	return NULL;
}


/* Setup the exported backend */
struct mime_backend mailcap_mime_backend = {
	NULL_LIST_HEAD,
	/* name: */		BACKEND_NAME,
	/* init: */		init_mailcap,
	/* done: */		done_mailcap,
	/* get_content_type: */	NULL,
	/* get_mime_handler: */	get_mime_handler_mailcap,
};

#endif /* MAILCAP */
