/* CSS token scanner utilities */
/* $Id: scanner.c,v 1.27 2004/01/19 23:21:09 jonas Exp $ */

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

static inline enum css_token_type
scan_css_token(struct css_scanner *scanner, struct css_token *token)
{
	unsigned char *string = scanner->position;
	unsigned char first_char = *string;

	token->string = string++;

	if (is_css_char_token(first_char)) {
		token->type = first_char;

	} else if (is_css_digit(first_char)) {
		scan_css(string, CSS_CHAR_DIGIT);

		if (*string == '%') {
			token->type = CSS_TOKEN_PERCENTAGE;
			string++;
		} else {
			token->type = CSS_TOKEN_DIGIT;
		}

	} else if (first_char == '#') {
		int hexdigits;

		scan_css(string, CSS_CHAR_HEX_DIGIT);

		/* Check that the hexdigit sequence is either 3 or 6 chars */
		hexdigits = string - token->string - 1;
		token->type = (hexdigits == 3 || hexdigits == 6)
			    ? CSS_TOKEN_HEX_COLOR : CSS_TOKEN_GARBAGE;

	} else if (first_char == '@') {

		/* Compose token containing @<ident> */
		if (is_css_ident_start(*string)) {
			/* Scan both ident start and ident */
			scan_css(string, CSS_CHAR_IDENT);
			token->type = CSS_TOKEN_ATRULE;

		} else {
			token->type = CSS_TOKEN_GARBAGE;
		}

	} else if (first_char == '"' || first_char == '\'') {
		/* TODO: Escaped delimiters --jonas */
		unsigned char *string_end = strchr(string, first_char);

		if (string_end) {
			string = string_end + 1;
			token->type = CSS_TOKEN_STRING;
		} else {
			token->type = CSS_TOKEN_GARBAGE;
		}

	} else if ((first_char == '-' && *string == '-' && string[1] == '>')
		   || (first_char == '<' && *string == '-' && string[1] == '-')) {
		/* SGML left and right comments */
		token->type = CSS_TOKEN_SGML_COMMENT;
		string += 2;

	} else if (is_css_ident(first_char)) {
		scan_css(string, CSS_CHAR_IDENT);

		if (!is_css_ident_start(first_char)) {
			token->type = CSS_TOKEN_NAME;

		} else if (*string == '(') {
			token->type = CSS_TOKEN_FUNCTION;
			string++;

		} else {
			token->type = CSS_TOKEN_IDENTIFIER;
		}

	} else {
		/* TODO: Better composing of error tokens. For now we just
		 * split them down into char tokens */
		if (!first_char) {
			string--;
			token->string = NULL;
			token->type = CSS_TOKEN_NONE;
		} else {
			token->type = CSS_TOKEN_GARBAGE;
		}
	}

	token->length = string - token->string;
	scanner->position = string;

	return token->type;
}

void
scan_css_tokens(struct css_scanner *scanner)
{
	struct css_token *table = scanner->table;
	int tokens = scanner->tokens;
	int move_to_front = int_max(tokens - scanner->current, 0);
	int current = move_to_front ? scanner->current : 0;

#ifdef CSS_SCANNER_DEBUG
	if (tokens > 0) WDBG("Rescanning");
#endif

	/* Move any untouched tokens */
	if (move_to_front) {
		int size = move_to_front * sizeof(struct css_token);

		memmove(table, &table[current], size);
	}

	/* Set all unused tokens to CSS_TOKEN_NONE */
	memset(&table[move_to_front], 0, sizeof(struct css_token) * (CSS_SCANNER_TOKENS - move_to_front));

	if (!*scanner->position) {
		scanner->tokens = move_to_front ? move_to_front : -1;
		scanner->current = 0;
		return;
	}

	/* Scan tokens til we have filled the table */
	for (current = move_to_front;
	     current < CSS_SCANNER_TOKENS && *scanner->position;
	     current++) {
		struct css_token *token = &table[current];

		scan_css(scanner->position, CSS_CHAR_WHITESPACE);

		if (scan_css_token(scanner, token) == CSS_TOKEN_NONE) {
			current--;
			break;
		}
	}

	scanner->tokens = current;
	scanner->current = 0;
}


/* Scanner table accessors and mutators */

#define check_css_scanner(scanner) \
	(scanner->tokens < CSS_SCANNER_TOKENS \
	 || scanner->current + 1 < scanner->tokens)

struct css_token *
get_css_token_(struct css_scanner *scanner)
{
	assert(scanner);

	/* If the scanners table is full make sure that last token skipping
	 * or get_next_css_token() call made it possible to check the type
	 * of the next token. */
	assert(check_css_scanner(scanner));

#ifdef CSS_SCANNER_DEBUG
	if (css_scanner_has_tokens(scanner)) {
		struct css_token *token = &scanner->table[scanner->current];

		errfile = scanner->file, errline = scanner->line;
		elinks_wdebug("<%s> %d %d [%s]", scanner->function, token->type,
			      token->length, token->string);
	}
#endif

	return css_scanner_has_tokens(scanner)
		? &scanner->table[scanner->current] : NULL;
}

struct css_token *
get_next_css_token_(struct css_scanner *scanner)
{
	scanner->current++;

	/* Do a scanning if we do not have also have access to next token */
	if (scanner->current + 1 >= scanner->tokens) {
		scan_css_tokens(scanner);
	}
	return get_css_token_(scanner);
}

struct css_token *
skip_css_tokens_(struct css_scanner *scanner, enum css_token_type type)
{
	struct css_token *token = get_css_token_(scanner);

	/* Skip tokens while handling some basic precedens of special chars
	 * so we don't skip to long. */
	while (token) {
		if (token->type == type
		    || (token->type == ';' && type == ':')
		    || (token->type == '}' && (type == ';' || type == ':')))
			break;
		token = get_next_css_token_(scanner);
	}

	return (token && token->type == type)
		? get_next_css_token_(scanner) : NULL;
}

int
check_next_css_token(struct css_scanner *scanner, enum css_token_type type)
{
	/* See comment about alignment of the scanners token table */
	assert(check_css_scanner(scanner));

	return css_scanner_has_tokens(scanner)
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
	for (chars = "({});:,"; *chars; chars++) {
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
	scanner->position = string;
	scan_css_tokens(scanner);
}
