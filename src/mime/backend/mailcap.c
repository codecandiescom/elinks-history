/* RFC1524 (mailcap file) implementation */
/* $Id: mailcap.c,v 1.14 2003/06/06 12:46:06 jonas Exp $ */

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

#include "mime/backend/common.h"
#include "mime/backend/mailcap.h"
#include "mime/mime.h"
#include "osdep/os_dep.h"		/* For exe() */
#include "sched/download.h"		/* For subst_file() */
#include "util/file.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"

#define BACKEND_NAME	"mailcap"

struct mailcap_entry {
	unsigned char *type;		/* base/type field. Use as hash key */
	unsigned char *command;		/* Ready for ses->tq_prog ;) */
	unsigned char *testcommand;	/* To verify if command qualifies */
	unsigned char *description;	/* Used as name for the handler */
	int needsterminal;		/* Assigned to "block" */
	int copiousoutput;		/* If "| ${PAGER}" should be added */
	int testneedsfile;		/* If testing requires a filename */
	unsigned int priority;		/* Increased for each sourced file */
	struct mailcap_entry *next;	/* If several handlers for one type */
};

/* State variables */
static int mailcap_map_entries = 0;
static struct hash *mailcap_map = NULL;


static inline void
done_mailcap_entry(struct mailcap_entry *entry)
{
	if (!entry) return;
	if (entry->command)	mem_free(entry->command);
	if (entry->testcommand)	mem_free(entry->testcommand);
	if (entry->type)	mem_free(entry->type);
	if (entry->description)	mem_free(entry->description);
	mem_free(entry);
}

/* Takes care of all initialization of mailcap entries including adding it to
 * the map. Clear memory to make freeing it safer later and we get
 * needsterminal and copiousoutput inialized for free. */
static inline struct mailcap_entry *
init_mailcap_entry(unsigned char *type, unsigned char *command, int priority)
{
	struct mailcap_entry *entry;
	struct hash_item *item;
	int typelen;

	entry = mem_calloc(1, sizeof(struct mailcap_entry));
	if (!entry)
		return NULL;

	typelen = strlen(type);
	entry->type = memacpy(type, typelen);
	if (!entry->type) {
		mem_free(entry);
		return NULL;
	}

	entry->command = stracpy(command);
	if (!entry->command) {
		done_mailcap_entry(entry);
		return NULL;
	}

	entry->priority = priority;

	/* Time to get the entry into the mailcap_map */

	/* First check if the type is already checked in */
	item = get_hash_item(mailcap_map, entry->type, typelen);
	if (item) {
		struct mailcap_entry *current = item->value;

		if (!current)
			return NULL;

		/* Add as the last item */
		while (current->next) current = current->next;
		current->next = entry;
	} else {
		if (!add_hash_item(mailcap_map, entry->type, typelen, entry)) {
			done_mailcap_entry(entry);
			return NULL;
		}
	}

	mailcap_map_entries++;
	return entry;
}


/* Parsing of a rfc1524 mailcap file */
/* The format is:
 *
 *	base/type; command; extradefs
 *
 * type can be * for matching all base with no /type is an implicit
 * wild command contains a %s for the filename to pass, default to pipe on
 * stdin extradefs are of the form:
 *
 *	def1="definition"; def2="define \;";
 *
 * line wraps with a \ at the end of the line, # for comments. */

/* Returns a NULL terminated rfc 1524 field, while modifying <next> to point
 * to the next field. */
static unsigned char *
get_field(unsigned char **next)
{
	unsigned char *field;
	unsigned char *p;

	if (!next || !*next) return NULL;

	field = *next;
	while (0xfed) { /* with chars */
		/* Get pointer to the next occurence of ; or \ */
		*next = strpbrk(field, ";\\");

		if (!*next) break;

		if (**next == '\\') {
			field = *next + 1;
			if (*field) field++;
		} else {
			*(*next) = 0;
			(*next)++;
			/* Skip whitespace */
			while (**next && isspace(**next)) (*next)++;
			break;
		}
	}
	/* Remove trailing whitespace */
	for (p = field + strlen(field) - 1; p >= field && isspace(*p); p--)
		*p = 0;

	return field;
}

/* Parses specific fields (ex: the '=TestCommand' part of 'test=TestCommand').
 * Expects that <field> is pointing right after the specifier (ex: 'test'
 * above). Allocates and returns a NULL terminated token, or NULL if parsing
 * fails. */

static unsigned char *
get_field_text(unsigned char *field)
{
	/* Skip whitespace */
	while (*field && isspace(*field)) field++;

	if (*field == '=') {
		field++;

		/* Skip whitespace */
		while (*field && isspace(*field)) field++;

		return stracpy(field);
	}

	return NULL;
}

/* Parse optional extra definitions. Zero return value means syntax error  */
static inline int
parse_optional_fields(struct mailcap_entry *entry, unsigned char *line)
{
	int error_code = 1;

	while (0xf131d5) {
		unsigned char *field = get_field(&line);

		if (!field) break;

		if (!strcasecmp(field, "needsterminal")) {
				entry->needsterminal = 1;

		} else if (!strcasecmp(field, "copiousoutput")) {
			entry->copiousoutput = 1;

		} else if (!strncasecmp(field, "test", 4)) {
			entry->testcommand = get_field_text(field + 4);
			if (!entry->testcommand) {
				error_code = 0;
				continue;
			}

			/* Find out wether testing requires filename */
			for (field = entry->testcommand; *field; field++)
				if (*field == '%' && *(field+1) == 's') {
					mem_free(entry->testcommand);
					entry->testcommand = NULL;
					entry->testneedsfile = 1;
					break;
				}

		} else if (!strncasecmp(field, "description", 11)) {
			field = get_field_text(field + 11);
			if (!field) {
				error_code = 0;
				continue;
			}

			entry->description = field;
		}
	}

	return error_code;
}

/* Parses hole mailcap files line by line adding entries to the map
 * assigning them the given @priority */
static void
parse_mailcap_file(unsigned char *filename, unsigned int priority)
{
	FILE *file = fopen(filename, "r");
	unsigned char *line = NULL;
	size_t linelen;
	int lineno = 0;

	if (!file) return;

	while ((line = file_read_line(line, &linelen, file, &lineno))) {
		struct mailcap_entry *entry;
		unsigned char *type;
		unsigned char *command;
		unsigned char *linepos;

		/* Ignore comments */
		if (*line == '#') continue;

		linepos = line;

		/* Get type */
		type = get_field(&linepos);
		if (!type) continue;

		/* Next field is the viewcommand */
		command = get_field(&linepos);
		if (!command) continue;

		entry = init_mailcap_entry(type, command, priority);

		if (!parse_optional_fields(entry, linepos)) {
			error("Bad formated entry for type %s in \"%s\" line %d",
			      entry->type, filename, lineno);
		}
	}

	fclose(file);
	if (line) mem_free(line); /* Alloced by file_read_line() */
}


/* When initializing mailcap subsystem we read, parse and build a hash mapping
 * content type to handlers. Map is build from a list of mailcap files.
 *
 * The rfc1524 specifies that a path of mailcap files should be used.
 *	o First we check to see if the user supplied any in mime.mailcap.path
 *	o Then we check the MAILCAP environment variable.
 *	o Finally fall back to reasonable default
 */

static void
init_mailcap(void)
{
	unsigned char *path;
	unsigned int priority = 0;

	if(!get_opt_bool("mime.mailcap.enable"))
		return; /* and leave mailcap_map = NULL */

	mailcap_map = init_hash(8, &strhash);

	/* Try to setup mailcap_path */
	path = get_opt_str("mime.mailcap.path");
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
	if (mailcap_map) {
		struct hash_item *item;
		int i;

		foreach_hash_item(item, *mailcap_map, i)
			if (item->value) {
				struct mailcap_entry *entry = item->value;

				while (entry) {
					struct mailcap_entry *next = entry->next;

					done_mailcap_entry(entry);
					entry = next;
				}
			}

		free_hash(mailcap_map);
	}

	mailcap_map = NULL;
	mailcap_map_entries = 0;
}


/* The command semantics include the following:
 *
 * %s		is the filename that contains the mail body data
 * %t		is the content type, like text/plain
 * %{parameter} is replaced by the parameter value from the content-type
 *		field
 * \%		is %
 *
 * Unsupported rfc1524 parameters: these would probably require some doing
 * by mutt, and can probably just be done by piping the message to metamail:
 *
 * %n		is the integer number of sub-parts in the multipart
 * %F		is "content-type filename" repeated for each sub-part
 * Only % is supported by subst_file() which is equivalent to %s. */

/* The formating is prosponed to when the command is needed. This means
 * @type can be NULL */
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

			if (*command == 's')
				add_chr_to_str(&cmd, &cmdlen, '%');
			else if (*command == 't' && type)
				add_to_str(&cmd, &cmdlen, type);

			command++;

		} else if (*command == '\\') {
			command++;
			if (*command)
				add_chr_to_str(&cmd, &cmdlen, *command++);
		}
	}

	if (copiousoutput) {
		/* Here we handle copiousoutput flag by appending $PAGER.
		 * It would of course be better to pipe the output into a
		 * buffer and let elinks display it but this will have to do
		 * for now. */
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
check_entries(struct mailcap_entry *entry)
{
	/* Use the list of entries to find a final match */
	for (; entry; entry = entry->next) {
		unsigned char *test;

		/* TODO Entries with testneedsfile should not be in the map */
		if (entry->testneedsfile)
			continue;

		/* Accept current if no test is needed */
		if (!entry->testcommand)
			break;

		/* We have to run the test command */
		test = format_command(entry->testcommand, NULL, 0);
		if (test) {
			int exitcode = exe(test);

			mem_free(test);
			if (!exitcode)
				break;
		}
	}

	return entry;
}

/* Attempts to find the given type in the mailcap association map.  On success,
 * this returns the associated command, else NULL.  Type is a string with
 * syntax '<base>/<type>' (ex: 'text/plain')
 *
 * First the given type is looked up. Then the given <base>-type with added
 * wildcard '*' (ex: 'text/<star>'). For each lookup all the associated
 * entries are checked/tested.
 *
 * The lookup support testing on files. If no file is given (NULL) any tests
 * that needs a file will be taken as failed. */

static struct mime_handler *
get_mime_handler_mailcap(unsigned char *type, int options)
{
	struct mailcap_entry *entry = NULL;
	struct hash_item *item;

	/* Check if mailcap support is disabled */
	if(!get_opt_bool("mime.mailcap.enable")) return NULL;

	/* If the map was not initialized do it now. */
	if (!mailcap_map) init_mailcap();

	/* First the given type is looked up. */
	item = get_hash_item(mailcap_map, type, strlen(type));

	/* Check list of entries */
	if (item && item->value)
		entry = check_entries(item->value);

	if (!entry || get_opt_bool("mime.mailcap.prioritize")) {
		/* The type lookup has either failed or we need to check
		 * the priorities so get the wild card handler */
		unsigned char *ptr;

		/* Find length of basetype */
		ptr = strchr(type, '/');
		if (ptr) {
			unsigned char *wildcardtype;
			int wildcardlen;

			wildcardlen = ptr - type + 1; /* including '/' */

			wildcardtype = mem_alloc(wildcardlen + 2);
			if (!wildcardtype) return NULL;
			memcpy(wildcardtype, type, wildcardlen);

			wildcardtype[wildcardlen++] = '*';
			wildcardtype[wildcardlen] = '\0';

			item = get_hash_item(mailcap_map, wildcardtype, wildcardlen);

			mem_free(wildcardtype);

			if (item && item->value) {
				struct mailcap_entry *wildcard;

				wildcard = check_entries(item->value);

				if (wildcard &&
			    	    (!entry ||
				     (wildcard->priority < entry->priority)))
					entry = wildcard;
			}
		}
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

		if (entry->needsterminal || entry->copiousoutput)
			handler->flags |= MIME_BLOCK;

		if (get_opt_bool("mime.mailcap.ask"))
			handler->flags |= MIME_ASK;

		handler->program = program;
		handler->description = entry->description;
		handler->backend_name = "Mailcap";

		return handler;
	}

	return NULL;
}

/* Setup the exported backend */
struct mime_backend mailcap_mime_backend = {
	/* init: */		init_mailcap,
	/* done: */		done_mailcap,
	/* get_content_type: */	NULL,
	/* get_mime_handler: */	get_mime_handler_mailcap,
};

#endif /* MAILCAP */
