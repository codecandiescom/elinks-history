/* RFC1524 (mailcap file) implementation */
/* $Id: mailcap.c,v 1.10 2003/04/29 08:34:11 zas Exp $ */

/*
 * This file contains various functions for implementing a fair subset of
 * rfc1524.
 *
 * The rfc1524 defines a format for the Multimedia Mail Configuration, which is
 * the standard mailcap file format under Unix which specifies what external
 * programs should be used to view/compose/edit multimedia files based on
 * content type.
 *
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (c) 2002      Jonas Fonseca <fonseca@diku.dk> for ELinks project
 *
 * This file was hijacked from the Mutt project <URL:http://www.mutt.org>
 * (version 1.4) on Saturday the 7th December 2002. It has been heavily
 * elinksified.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "protocol/mailcap.h"
#include "sched/download.h"
#include "util/file.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"

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


static struct mailcap_entry *
mailcap_new_entry(void)
{
	/*
	 * Clear memory to make freeing it safer laterand we get
	 * needsterminal and copiousoutput inialized for free.
	 */
	return  mem_calloc(1, sizeof(struct mailcap_entry));
}

static void
mailcap_free_entry(struct mailcap_entry *entry)
{
	if (!entry) return;
	if (entry->command)	mem_free(entry->command);
	if (entry->testcommand)	mem_free(entry->testcommand);
	if (entry->type)	mem_free(entry->type);
	if (entry->description)	mem_free(entry->description);
	mem_free(entry);
}


/*
 * The command semantics include the following:
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
 *
 * ELinks supports just % in mime types which is equivalent to %s.
 */

static unsigned char *
convert_command(unsigned char *command, int copiousoutput)
{
	int x = 0, y = 0; /* x used as command index, y as buffer index */
	unsigned char buffer[MAX_STR_LEN];
	unsigned char *converted;
	int commandlen = strlen(command);

	while (command[x] && x < commandlen && y < sizeof(buffer)) {
		if (command[x] == '\\') {
			x++;
			buffer[y++] = command[x++];

		} else if (command[x] == '%') {
			x++;
			if (command[x] == 's') {
				buffer[y++] = '%';
				x++;
			} else if (command[x] == 't') {
				buffer[y++] = '%';
				buffer[y++] = 't';
				x++;
			} else {
				/* Skip parameter - until next whitespace */
				while (command[x] && !isspace(command[x])) x++;
			}

		} else {
			buffer[y++] = command[x++];
		}
	}

	if (copiousoutput) {
		/*
		 * Here we handle copiousoutput flag by appending $PAGER.
		 * It would of course be better to pipe the output into a
		 * buffer and let elinks display it but this will have to do
		 * for now.
		 */
		unsigned char *pager = getenv("PAGER");

		if (!pager && file_exists(DEFAULT_PAGER_PATH)) {
			pager = DEFAULT_PAGER_PATH;
		} else if (!pager && file_exists(DEFAULT_LESS_PATH)) {
			pager = DEFAULT_LESS_PATH;
		} else if (!pager && file_exists(DEFAULT_MORE_PATH)) {
			pager = DEFAULT_MORE_PATH;
		}

		if (pager) {
			buffer[y++] = ' ';
			buffer[y++] = '|';
			safe_strncpy(buffer + y, pager, sizeof(buffer) - y);
			y += strlen(pager);
		}
	}
	buffer[y++] = '\0';

	converted = mem_alloc(y);
	if (converted)
		safe_strncpy(converted, buffer, y);

	return converted;
}

/*
 * Returns a NULL terminated rfc 1524 field, while modifying <next> to point
 * to the next field.
 */
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

/*
 * Parses specific fields (ex: the '=TestCommand' part of 'test=TestCommand').
 * Expects that <field> is pointing right after the specifier (ex: 'test'
 * above). Allocates and returns a NULL terminated token, or NULL if parsing
 * fails.
 */

static unsigned char *
get_field_text(unsigned char *field,
	       unsigned char *type,
	       unsigned char *filename,
	       int lineno)
{
	/* Skip whitespace */
	while (*field && isspace(*field)) field++;

	if (*field == '=') {
		field++;

		/* Skip whitespace */
		while (*field && isspace(*field)) field++;

		return stracpy(field);
	}

	error("Bad formated entry for type %s in \"%s\" line %d\n",
	      type, filename, lineno);

	return NULL;
}

/*
 * rfc1524 mailcap file is of the format:
 *
 *	base/type; command; extradefs
 *
 * type can be * for matching all base with no /type is an implicit
 * wild command contains a %s for the filename to pass, default to pipe on
 * stdin extradefs are of the form:
 *
 *	def1="definition"; def2="define \;";
 *
 * line wraps with a \ at the end of the line, # for comments.
 */

static void
mailcap_parse(unsigned char *filename, unsigned int priority)
{
	FILE *file;
	unsigned char *line = NULL;
	size_t linelen;
	int lineno = 0;
	struct mailcap_entry *entry = NULL;
	size_t typelen;
	unsigned char *field;	/* Points to the current field */
	unsigned char *next;	/* Points to the next field */

	file = fopen(filename, "r");
	if (!file) return;

	while ((line = file_read_line(line, &linelen, file, &lineno))) {
		/* Ignore comments */
		if (*line == '#') continue;

		/* Clean up from previous iterations */
		/* This could of course be better since we alloc right after */
		if (entry) mailcap_free_entry(entry);

		/* Create the next entry */
		entry = mailcap_new_entry();
		if (!entry) break;

		entry->needsterminal = 0; /* Redundant since mailcap_new_entry */
		entry->copiousoutput = 0; /* clears all. Here for clairity */

		next = line;

		/* Get type */
		field = get_field(&next);
		if (!field) continue;

		typelen     = strlen(field);
		entry->type = stracpy(field);
		if (!entry->type) continue;

		/* Next field is the viewcommand */
		field = get_field(&next);
		if (!field) continue;
		entry->command = field;

		/* Parse the optional fields */
		while (field) {
			field = get_field(&next);
			if (!field) break;

			if (!strcasecmp(field, "needsterminal")) {
				entry->needsterminal = 1;

			} else if (!strcasecmp(field, "copiousoutput")) {
				entry->copiousoutput = 1;

			} else if (!strncasecmp(field, "test", 4)) {
				field = get_field_text(field + 4, entry->type,
					               filename, lineno);
				if (!field) continue;

				entry->testcommand = convert_command(field, 0);
				mem_free(field);

				/* Find out wether testing requires filenam */
				field = entry->testcommand;
				for (field = entry->testcommand; *field; field++) {
					if (*field == '%') {
						entry->testneedsfile = 1;
						break;
					}
				}
			} else if (!strncasecmp(field, "description", 11)) {
				field = get_field_text(field + 11, entry->type,
					               filename, lineno);
				if (!field) continue;

				entry->description = field;

			}
			/* Other optional fields are not currently useful */
		}

		/* Keep after parsing of optional fields (hint: copiousoutput) */
		entry->command = convert_command(entry->command,
						 entry->copiousoutput);

		entry->priority = priority;

		/* Time to get the entry into the mailcap_map */
		if (entry->command) {
			struct hash_item *item;

			/* First check if the type is already checked in */
			item = get_hash_item(mailcap_map, entry->type, typelen);
			if (!item) {
				if (!add_hash_item(mailcap_map, entry->type, typelen, entry)) {
					mailcap_free_entry(entry);
					continue;
				}
			} else {
				struct mailcap_entry *current = item->value;

				if (!current) continue;

				/* Add as the last item */
				while (current->next) current = current->next;
				current->next = entry;
			}
			mailcap_map_entries++;

			/* XXX: Nothing should be 'auto'-freed */
			entry = NULL;
		}
	}

	/* Clean up from previous iterations */
	if (entry) mailcap_free_entry(entry);

	fclose (file);
	if (line) mem_free(line); /* Alloced by file_read_line() */
}


/*
 * When initializing mailcap subsystem we read, parse and build a hash mapping
 * content type to handlers. Map is build from a list of mailcap files.
 *
 * The rfc1524 specifies that a path of mailcap files should be used.
 * - First we check to see if the user supplied any in protocol.mailcap.path
 * - Then we check the MAILCAP environment variable.
 * - Finally fall back to reasonable defaults like say
 *
 * 	~/mailcap:/etc/mailcap
 *
 *   Users own file take precedence.
 */

void
mailcap_init()
{
	unsigned char *path;
	unsigned int priority = 0;

	if(!get_opt_bool("protocol.mailcap.enable"))
		return; /* and leave mailcap_map = NULL */

	mailcap_map = init_hash(8, &strhash);

	/* Try to setup mailcap_path */
	path = get_opt_str("protocol.mailcap.path");
	if (!path || !*path) path = getenv("MAILCAP");
	if (!path) path = DEFAULT_MAILCAP_PATH;

	while (*path) {
		unsigned char file[MAX_STR_LEN];
		unsigned char *expanded;
		int indx = 0;

		/* Extract file from path */
		while (*path && *path != ':' && indx < sizeof(file) - 1) {
			file[indx++] = *path;
			path++;
		}
		if (*path) path++;

		if (!indx) continue; /* No file extracted */

		file[indx] = '\0';
		expanded = expand_tilde(file);
		if (!expanded) continue; /* Bad allocation */

		if (expanded != file) {
			safe_strncpy(file, expanded, sizeof(file));
			mem_free(expanded);
		}

		mailcap_parse(file, priority++);
	}
}

void
mailcap_exit()
{
	if (mailcap_map) {
		struct hash_item *item;
		int i;

		/* We do not free key here. */
		foreach_hash_item(item, *mailcap_map, i)
			if (item->value) {
				struct mailcap_entry *entry = item->value;

				while (entry) {
					struct mailcap_entry *next = entry->next;

					mailcap_free_entry(entry);
					entry = next;
				}
			}

		free_hash(mailcap_map);
	}

	mailcap_map = NULL;
	mailcap_map_entries = 0;
}


/* Basicly this is just convert_command() handling only %t */
static unsigned char *
expand_command(unsigned char *command, unsigned char *type)
{
	int x = 0; /* command index */
	int y = 0; /* buffer index */
	unsigned char buffer[MAX_STR_LEN];
	unsigned char *expanded;
	int commandlen;
	int typelen;

	if (!command) return NULL;

	typelen = strlen(type);
	commandlen = strlen(command);

	while (command[x] && x < commandlen && y < sizeof(buffer)) {
		if (command[x] == '%' && command[x + 1] == 't') {
			safe_strncpy(buffer + y, type, sizeof(buffer) - y);
			y += typelen;
			x += 2;
		} else {
			buffer[y++] = command[x++];
		}
	}
	buffer[y++] = '\0';

	expanded = mem_alloc(y);
	if (expanded)
		safe_strncpy(expanded, buffer, y);

	return expanded;
}

/*
 * Build an option from a mailcap entry. This way mailcap implementation can
 * mimic the general option based mime handling. Only build the absolutely
 * nescessary.
 */

static struct option *
convert2option(struct mailcap_entry *entry, unsigned char *type)
{
	struct option *association;
	unsigned char *command;
	int ask;
	int block;

	if (!entry) return NULL;
	association = mem_alloc(sizeof(struct option));
	if (!association) return NULL;

	init_list(*association);
	switch (get_opt_bool("protocol.mailcap.description")) {
		case 1:
			association->name = stracpy(entry->command);
			break;
		case 2:
			if (entry->description) {
				association->name = stracpy(entry->description);
				break;
			}
		default:
			/* XXX: Possibly fall through to provide fallback */
			association->name = stracpy("mailcap");
	}

	if (!association->name) {
		mem_free(association);
		return NULL;
	}

	association->box_item = NULL;
	association->type     = OPT_TREE;
	association->ptr      = init_options_tree();

	command = expand_command(entry->command, type);
	if (!command) {
		mem_free(association);
		return NULL;
	}

	ask	= get_opt_bool("protocol.mailcap.ask");
	block	= entry->needsterminal;
	if (entry->copiousoutput) block	= entry->copiousoutput;

	add_opt_string_tree(association, "", NULL, "program", 0, command, NULL);
	add_opt_bool_tree(association, "", NULL, "block", 0, block, NULL);
	add_opt_bool_tree(association, "", NULL, "ask", 0, ask, NULL);

	if (command) mem_free(command);
	return association;
}

/* Checks a single linked list of entries by running test commands if any */
static struct mailcap_entry *
check_entries(struct mailcap_entry * entry, unsigned char *filename)
{
	/* Use the list of entries to find a final match */
	while (entry) {
		if (entry->testcommand) {
			/*
			 * This routine executes the given test command to
			 * determine if this is the right match.
			 */

			/* Use filename as marker to wether test should run */
			if (!entry->testneedsfile && !filename) filename = "";

			if (filename) {
				int exitcode = 1;
				unsigned char *testcommand;

				testcommand = subst_file(entry->testcommand, filename);
				if (testcommand) {
					exitcode = exe(testcommand);
					mem_free(testcommand);
				}

				/* A non-zero exit code means test failed */
				if (!exitcode) return entry;
			}
		} else {
			if (entry->command) return entry;
		}
		entry = entry->next;
	}

	return NULL;
}

/*
 * Attempts to find the given type in the mailcap association map.  On success,
 * this returns the associated command, else NULL.  Type is a string with
 * syntax '<base>/<type>' (ex: 'text/plain')
 *
 * First the given type is looked up. Then the given <base>-type with added
 * wildcard '*' (ex: 'text/<star>'). For each lookup all the associated
 * entries are checked/tested.
 *
 * The lookup support testing on files. If no file is given (NULL) any tests
 * that needs a file will be taken as failed.
 */

struct option *
mailcap_lookup(unsigned char *type, unsigned char *file)
{
	struct mailcap_entry *entry = NULL;
	struct hash_item *item;

	/* Check if mailcap support is disabled */
	if (!mailcap_map) return NULL;

	/* First the given type is looked up. */
	item = get_hash_item(mailcap_map, type, strlen(type));

	/* Check list of entries */
	if (item && item->value) entry = check_entries(item->value, file);

	if (!entry || get_opt_bool("protocol.mailcap.prioritize")) {
		/* The type lookup has either failed or we need to check
		 * the priorities so get the wild card handler */
		unsigned char *ptr;

		/* Find length of basetype */
		ptr = strchr(type, '/');
		if (ptr) {
			struct mailcap_entry *wildcard = NULL;
			unsigned char *wildcardtype;
			int wildcardlen;

			wildcardlen = ptr - type + 1; /* including '/' */

			wildcardtype = mem_alloc(wildcardlen + 3);
			if (!wildcardtype) return NULL;
			safe_strncpy(wildcardtype, type, wildcardlen + 1);

			wildcardtype[wildcardlen++] = '*';
			wildcardtype[wildcardlen] = '\0';

			item = get_hash_item(mailcap_map, wildcardtype, wildcardlen);

			mem_free(wildcardtype);

			if (item && item->value)
				wildcard = check_entries(item->value, file);

			if (entry && wildcard) {
				/* Do the actual prioritizing */
				if (wildcard->priority < entry->priority)
					entry = wildcard;
			} else {
				entry = wildcard;
			}
		}
	}

	return convert2option(entry, type);
}
