/* CSS token scanner utilities */
/* $Id: scanner.c,v 1.4 2004/01/18 17:09:57 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "document/css/scanner.h"
#include "util/error.h"


#define	SCAN_TABLE_SIZE	256

/* The scanner table */
/* Contains bitmaps for the various CSS characters groups.
 * Idea sync'ed from mozilla browser. */
static int css_scan_table[SCAN_TABLE_SIZE];

/* Bitmap entries for the CSS character groups used in the scanner table */

enum css_char_group {
	CSS_CHAR_ALPHA		= (1 << 0),
	CSS_CHAR_DIGIT		= (1 << 1),
	CSS_CHAR_HEX_DIGIT	= (1 << 2),
	CSS_CHAR_IDENT_START	= (1 << 3),
	CSS_CHAR_IDENT		= (1 << 4),
	CSS_CHAR_WHITESPACE	= (1 << 5),
	CSS_CHAR_NEWLINE	= (1 << 6),
	CSS_CHAR_NON_ASCII	= (1 << 7),
	CSS_CHAR_TOKEN		= (1 << 8),
};

#define	check_css_table(c, bit)	(css_scan_table[(c)] & (bit))
#define	is_css_ident_start(c)	check_css_table(c, CSS_CHAR_IDENT_START)
#define	is_css_ident(c)		check_css_table(c, CSS_CHAR_IDENT)
#define	is_css_digit(c)		check_css_table(c, CSS_CHAR_DIGIT)
#define	is_css_hexdigit(c)	check_css_table(c, CSS_CHAR_HEX_DIGIT)
#define	is_css_char_token(c)	check_css_table(c, CSS_CHAR_TOKEN)
#define	scan_css(s, bit)	while (check_css_table(*(s), bit)) (s)++;

static inline int
scan_css_token(struct css_token *token, unsigned char *string)
{
	unsigned char first_char = *string;

	token->string = string++;

	if (is_css_char_token(first_char)) {
		token->type = first_char;

	} else if (is_css_ident(first_char)) {
		token->type = is_css_ident_start(first_char)
			    ? CSS_TOKEN_IDENTIFIER : CSS_TOKEN_NAME;
		scan_css(string, CSS_CHAR_IDENT);

	} else if (is_css_digit(first_char)) {
		scan_css(string, CSS_CHAR_DIGIT);

		token->type = (*string == '%')
			    ? CSS_TOKEN_PERCENTAGE : CSS_TOKEN_DIGIT;

	} else if (first_char == '#') {
		token->type = CSS_TOKEN_HEX_COLOR;
		scan_css(string, CSS_CHAR_HEX_DIGIT);
		/* FIXME: Check that it is either 3 or 6 chars */

	} else {
		/* TODO: Strings and maybe function token having "<ident>(" */
		/* TODO: Better composing of error tokens. For now we just
		 * split them down into char tokens */
		if (!first_char) {
			string--;
			token->type = CSS_TOKEN_NONE;
		} else {
			token->type = CSS_TOKEN_GARBAGE;
		}
	}

	token->length = string - token->string;

	return token->length;
}

void
scan_css_tokens(struct css_scanner *scanner)
{
	struct css_token *table = scanner->table;
	int tokens = scanner->tokens;
	int current = (scanner->current < tokens) ? scanner->current : 0;
	unsigned char *string = tokens > 0
			      ? get_css_token_end(&table[tokens - 1])
			      : scanner->string;

#ifdef CSS_SCANNER_DEBUG
	if (tokens > 0) WDBG("Rescanning");
#endif

	/* Move any untouched tokens */
	if (current) {
		int size = (tokens - current) * sizeof(struct css_token);

		memmove(table, &table[current], size);
	}

	/* Set all tokens to CSS_TOKEN_NONE */
	memset(&table[current], 0, sizeof(struct css_token) * (CSS_SCANNER_TOKENS - current));

	if (tokens && tokens < CSS_SCANNER_TOKENS) {
		scanner->tokens = -1;
		scanner->current = 0;
		return;
	}

	tokens -= current;

	/* Scan tokens til we have filled the table */
	for (; current < CSS_SCANNER_TOKENS && *string; current++) {
		struct css_token *token = &table[current];

		scan_css(string, CSS_CHAR_WHITESPACE);

		if (!scan_css_token(token, string)) break;

		string += token->length;
	}

	scanner->tokens = current;
	scanner->current = 0;
}


/* Scanner table accessors and mutators */

struct css_token *
get_css_token_(struct css_scanner *scanner, unsigned char *file, int line)
{
	assert(scanner);

#ifdef CSS_SCANNER_DEBUG
	if (scanner->tokens) {
		struct css_token *token = &scanner->table[scanner->current];

		errfile = file, errline = line;
		elinks_wdebug("%d %d [%s]", token->type, token->length, token->string);
	}
#endif

	return scanner->tokens > 0 ? &scanner->table[scanner->current] : NULL;
}

struct css_token *
get_next_css_token_(struct css_scanner *scanner, unsigned char *file, int line)
{
	scanner->current++;
	if (scanner->current >= scanner->tokens) {
		scan_css_tokens(scanner);
	}
	return get_css_token_(scanner, file, line);
}

struct css_token *
skip_css_tokens_(struct css_scanner *scanner, enum css_token_type type,
		 unsigned char *file, int line)
{
	struct css_token *token = get_css_token_(scanner, file, line);

	/* TODO: Precedens handling. Stop if ';' is encountered while skipping
	 * for ':' and if '{' or '}' while skipping for ';' */
	while (token && token->type != type)
		token = get_next_css_token_(scanner, file, line);

	return (token && token->type == type)
		? get_next_css_token_(scanner, file, line) : NULL;
}
#define skip_css_tokens(scanner, type)  skip_css_tokens_(scanner, type, __FILE__, __LINE__)

int
check_next_css_token(struct css_scanner *scanner, enum css_token_type type)
{
	if (scanner->current + 1 >= scanner->tokens)
		scan_css_tokens(scanner);

	return scanner->current + 1 < scanner->tokens
		&& scanner->table[scanner->current + 1].type == type;
}


/* Initializers */

/* Initiate bitmaps */
static void
init_css_scan_table(void)
{
	unsigned char *chars;
	int index;

	memset(css_scan_table, 0, sizeof(css_scan_table));

	/* Unicode escape (that we do not handle yet) + other special chars */
	css_scan_table['\\'] = CSS_CHAR_IDENT | CSS_CHAR_IDENT_START;
	css_scan_table['_'] |= CSS_CHAR_IDENT | CSS_CHAR_IDENT_START;
	css_scan_table['-'] |= CSS_CHAR_IDENT;

	/* Whitespace chars */
	css_scan_table[' ']  |= CSS_CHAR_WHITESPACE;
	css_scan_table['\t'] |= CSS_CHAR_WHITESPACE;
	css_scan_table['\v'] |= CSS_CHAR_WHITESPACE;
	css_scan_table['\r'] |= CSS_CHAR_WHITESPACE | CSS_CHAR_NEWLINE;
	css_scan_table['\n'] |= CSS_CHAR_WHITESPACE | CSS_CHAR_NEWLINE;
	css_scan_table['\f'] |= CSS_CHAR_WHITESPACE | CSS_CHAR_NEWLINE;

	for (index = 161; index <= 255; index++) {
		css_scan_table[index] |= CSS_CHAR_NON_ASCII | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START;
	}

	for (index = '0'; index <= '9'; index++) {
		css_scan_table[index] |= CSS_CHAR_DIGIT | CSS_CHAR_HEX_DIGIT | CSS_CHAR_IDENT;
	}

	for (index = 'A'; index <= 'Z'; index++) {
		if ((index >= 'A') && (index <= 'F')) {
			css_scan_table[index]	   |= CSS_CHAR_HEX_DIGIT;
			css_scan_table[index + 32] |= CSS_CHAR_HEX_DIGIT;
		}

		css_scan_table[index]	   |= CSS_CHAR_ALPHA | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START;
		css_scan_table[index + 32] |= CSS_CHAR_ALPHA | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START;
	}

	/* This should contain mostly used char tokens like ':' and maybe a few
	 * garbage chars that people might put in their css code */
	for (chars = "({});:"; *chars; chars++) {
		css_scan_table[*chars] |= CSS_CHAR_TOKEN;
	}
}

void
init_css_scanner(struct css_scanner *scanner, unsigned char *string)
{
	static int init_scan_table;

	if (!init_scan_table) {
		init_css_scan_table();
		init_scan_table = 1;
	}

	memset(scanner, 0, sizeof(struct css_scanner));

	scanner->string = string;
	scan_css_tokens(scanner);
}
