/* Tool for testing the CSS parser */
/* $Id: test.c,v 1.1 2003/02/25 14:15:50 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define LEAK_DEBUG /* Placed here to get the right behaviour from utils */

#include "elusive/parser/css/parser.h"
#include "elusive/parser/css/scanner.h"
#include "elusive/parser/css/tree.h"
#include "elusive/parser/parser.h"
#include "util/error.h"
#include "util/string.h"
#include "util/memdebug.h"
#include "util/memory.h"

#define PEEKSIZE 10

static int peeksize = PEEKSIZE;
static int escape = 1;
int css_stack_size = 0;

struct state_info {
	enum css_state_code code;
	unsigned char *name;
};

struct state_info stateinfo[] = {
	{ CSS_ATRULE, "CSS_ATRULE" },
	{ CSS_CHARSET, "CSS_CHARSET" },
	{ CSS_COMMENT, "CSS_COMMENT" },
	{ CSS_DECLARATION, "CSS_DECLARATION" },
	{ CSS_DECLARATIONS, "CSS_DECLARATIONS" },
	{ CSS_ESCAPE, "CSS_ESCAPE" },
	{ CSS_EXPRESSION, "CSS_EXPRESSION" },
	{ CSS_FUNCTION, "CSS_FUNCTION" },
	{ CSS_IDENT, "CSS_IDENT" },
	{ CSS_IMPORT, "CSS_IMPORT" },
	{ CSS_MEDIA, "CSS_MEDIA" },
	{ CSS_MEDIATYPES, "CSS_MEDIATYPES" },
	{ CSS_NAME, "CSS_NAME" },
	{ CSS_PAGE, "CSS_PAGE" },
	{ CSS_RGB, "CSS_RGB" },
	{ CSS_RULESET, "CSS_RULESET" },
	{ CSS_SELECTOR, "CSS_SELECTOR" },
	{ CSS_SIMPLE_SELECTOR, "CSS_SIMPLE_SELECTOR" },
	{ CSS_SELECTOR_ATTR, "CSS_SELECTOR_ATTR" },
	{ CSS_SKIP, "CSS_SKIP" },
	{ CSS_SKIP_MEDIATYPES, "CSS_SKIP_MEDIATYPES" },
	{ CSS_SKIP_UNTIL, "CSS_SKIP_UNTIL" },
	{ CSS_STRING, "CSS_STRING" },
	{ CSS_STYLESHEET, "CSS_STYLESHEET" },
	{ CSS_UNICODERANGE, "CSS_UNICODERANGE" },
	{ CSS_URL, "CSS_URL" },
	{ 0, NULL }
};

unsigned char *
code2name(enum css_state_code code)
{
	int index;

	for (index = 0; index < CSS_STATE_CODES; index++) {
		if (stateinfo[index].code == code)
			return stateinfo[index].name;
	}
	return "UNKNOWN";	
}

void
print_state(struct parser_state *state, unsigned char *src, int len)
{
	struct css_parser_state *item = state->data;
	unsigned char *stackdump = init_str();
	unsigned char *srcpeek;
	int peekchars = peeksize;

	/* Builds string displaying the stack layout
	 * CSS_STYLESHEET CSS_RULESET CSS_SELECTOR etc. */
	while (item) {
		unsigned char *old = stackdump;

		stackdump = init_str();
		add_to_strn(&stackdump, code2name(item->state));
		add_to_strn(&stackdump, " ");
		add_to_strn(&stackdump, old);
		mem_free(old);
		item = item->up;
	}

	if (peekchars > len) peekchars = len;
	srcpeek = mem_alloc(peekchars+1);
	if (!srcpeek) { mem_free(stackdump); return; }

	if (escape) {
		int i = 0;
		int j = 0;

		for ( ; i < peekchars; i++, j++) {
			if (src[j] == '\n') {
				srcpeek[i++] = '\\';
				srcpeek[i] = 'n';
			} else {
				srcpeek[i] = src[j];
			}
		}
		srcpeek[i] = '\0';
	} else {
		safe_strncpy(srcpeek, src, peekchars + 1);
	}

	printf("%s[%s] %d\n", stackdump, srcpeek, len);
	mem_free(stackdump);
	mem_free(srcpeek);
}

void
print_token(unsigned char *desc, unsigned char *src, int len)
{
	unsigned char *buffer = mem_alloc(len + 1);

	if (buffer) {
		safe_strncpy(buffer, src, len + 1);
		printf("%s: contains [%s] length [%d]\n", desc, buffer, len);
		mem_free(buffer);
	}
}

void
print_css_node(unsigned char *desc, struct css_node *node)
{
	unsigned char *buffer = mem_alloc(node->strlen + 1);

	if (buffer) {
		safe_strncpy(buffer, node->str, node->strlen + 1);
		printf("%s %s\n", desc, buffer);
		mem_free(buffer);
	}
}

/* Utils for parsing various inputs */

static void
parse_string(struct parser_state *pstate, unsigned char *src, int dumpbuffer)
{
	int srclen = strlen(src);

	if (dumpbuffer) {
		printf("----------------------------------------------------\n");
		printf("%s\n", src);
		printf("----------------------------------------------------\n");
	}

	elusive_parser_parse(pstate, &src, &srclen);
}

static void
parse_file(struct parser_state *pstate, unsigned char *filename, int dumpbuffer)
{
	FILE *file;
	struct stat fileinfo;
	int filesize;
	int readsize;

	unsigned char *buffer;
	int bufferlen = 0;

	unsigned char *src;
	int srclen = 0;

	/* Initialize */
	file = fopen (filename, "r");
	if (!file) return;

	stat(filename, &fileinfo);
	filesize = fileinfo.st_size;
	buffer   = mem_alloc(filesize);
	if (!buffer) { fclose(file); return; }

	src = buffer;

	printf("File: %s\n", filename);

	while ((readsize = fread(&buffer[bufferlen], 1, 512, file))) {
		bufferlen += readsize;
		srclen	  += readsize;

		if (dumpbuffer) {
			unsigned char endchar =	buffer[bufferlen];

			buffer[bufferlen] = '\0';
			printf("----------------------------------------------------\n");
			printf("%s\n", src);
			printf("----------------------------------------------------\n");
			buffer[bufferlen] = endchar;
		}

		elusive_parser_parse(pstate, &src, &srclen);
	}

	mem_free(buffer);
	fclose(file);
}

int
main(int argc, char *argv[])
{
	int index = 1;
	int dumpbuffer = 0;
	void (*parse)(struct parser_state *, unsigned char *, int);

	css_init_scan_table();
	parse = parse_file;

	while (index < argc && argv[index][0] == '-') {
		if (!strncasecmp(argv[index], "--dump", 6)) {
			dumpbuffer = 1;
			index++;
			continue;
		}

		if (!strncasecmp(argv[index], "--string", 7)) {
			parse = parse_string;
			index++;
			continue;
		}

		if (!strncasecmp(argv[index], "--peeksize", 10)) {
			index++;
			peeksize = atoi(argv[index++]);
			continue;
		}

		if (!strncasecmp(argv[index], "--noescape", 10)) {
			escape = 0;
			index++;
			continue;
		}
	}

	if (index < argc) {
		struct parser_state *state = elusive_parser_init(PARSER_CSS);

		if (state) {
			parse(state, argv[index], dumpbuffer);
			elusive_parser_done(state);
		}
	}

	check_memory_leaks();
	return 0;
}
