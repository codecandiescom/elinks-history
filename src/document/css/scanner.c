/* CSS token scanner utilities */
/* $Id: scanner.c,v 1.52 2004/01/20 17:49:41 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "document/css/scanner.h"
#include "util/error.h"
#include "util/string.h"


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

struct css_number_identifier {
	unsigned char *name;
	enum css_token_type type;
};

static inline enum css_token_type
get_number_identifier(unsigned char *ident, int length)
{
	static struct css_number_identifier number_identifiers[] = {
		{ "Hz",   CSS_TOKEN_FREQUENCY },
		{ "cm",	  CSS_TOKEN_LENGTH },
		{ "deg",  CSS_TOKEN_ANGLE },
		{ "em",   CSS_TOKEN_EM },
		{ "ex",   CSS_TOKEN_EX },
		{ "grad", CSS_TOKEN_ANGLE },
		{ "in",   CSS_TOKEN_LENGTH },
		{ "kHz",  CSS_TOKEN_FREQUENCY },
		{ "mm",   CSS_TOKEN_LENGTH },
		{ "mm",   CSS_TOKEN_LENGTH },
		{ "ms",   CSS_TOKEN_TIME },
		{ "pc",   CSS_TOKEN_LENGTH },
		{ "pt",   CSS_TOKEN_LENGTH },
		{ "px",   CSS_TOKEN_LENGTH },
		{ "rad",  CSS_TOKEN_ANGLE },
		{ "s",    CSS_TOKEN_TIME },

		{ NULL, CSS_TOKEN_NONE },
	};
	int i;

	for (i = 0; number_identifiers[i].name; i++) {
		unsigned char *name = number_identifiers[i].name;

		if (!strncasecmp(name, ident, length))
			return number_identifiers[i].type;
	}

	return CSS_TOKEN_DIMENSION;
}


/* Check whéther it is safe to skip the char @c when looking for @skipto */
#define check_css_precedence(c, skipto)						\
	!(((skipto) == ':' && ((c) == ';' || (c) == '{' || (c) == '}'))		\
	  || ((skipto) == ')' && ((c) == ';' || (c) == '{' || (c) == '}'))	\
	  || ((skipto) == ';' && ((c) == '{' || (c) == '}'))			\
	  || ((skipto) == '{' && (c) == '}'))

#define	skip_css(s, skipto) \
	while (*(s) && check_css_precedence(*(s), skipto)) {			\
		if (*(s) == '"' || *(s) == '\'') {				\
			unsigned char *end = strchr(s + 1, *(s));		\
										\
			if (end) (s) = end;					\
		}								\
		(s)++;								\
	}

static inline void
scan_css_token(struct css_scanner *scanner, struct css_token *token)
{
	unsigned char *string = scanner->position;
	unsigned char first_char = *string;
	enum css_token_type type = CSS_TOKEN_GARBAGE;

	assert(first_char);
	token->string = string++;

	if (is_css_char_token(first_char)) {
		type = first_char;

	} else if (is_css_digit(first_char)) {
		scan_css(string, CSS_CHAR_DIGIT);

		/* First scan the full number token */
		if (*string == '.') {
			string++;

			if (is_css_digit(*string)) {
				type = CSS_TOKEN_NUMBER;
				scan_css(string, CSS_CHAR_DIGIT);
			}
		}

		/* Check what kind of number we have */
		if (*string == '%') {
			type = CSS_TOKEN_PERCENTAGE;
			string++;

		} else if (!is_css_ident_start(*string)) {
			type = CSS_TOKEN_NUMBER;

		} else {
			unsigned char *ident = string;

			scan_css(string, CSS_CHAR_IDENT);
			type = get_number_identifier(ident, string - ident);
		}

	} else if (first_char == '#') {
		/* Check wether hexcolor or hash token */
		if (is_css_hexdigit(*string)) {
			int hexdigits;

			scan_css(string, CSS_CHAR_HEX_DIGIT);

			/* Check that the hexdigit sequence is either 3 or 6 chars */
			hexdigits = string - token->string - 1;
			if (hexdigits == 3 || hexdigits == 6) {
				type = CSS_TOKEN_HEX_COLOR;
			} else {
				type = CSS_TOKEN_HASH;
			}

		} else if (is_css_ident(*string)) {
			/* Not *_ident_start() because hashes are #<name>. */
			scan_css(string, CSS_CHAR_IDENT);
			type = CSS_TOKEN_HASH;
		}

	} else if (first_char == '@') {
		/* Compose token containing @<ident> */
		if (is_css_ident_start(*string)) {
			/* Scan both ident start and ident */
			scan_css(string, CSS_CHAR_IDENT);
			type = CSS_TOKEN_ATRULE;
		}

	} else if (first_char == '!') {
		scan_css(string, CSS_CHAR_WHITESPACE);
		if (!strncasecmp(string, "important", 9)) {
			type = CSS_TOKEN_IMPORTANT;
			string += 9;
		}

	} else if (first_char == '"' || first_char == '\'') {
		/* TODO: Escaped delimiters --jonas */
		unsigned char *string_end = strchr(string, first_char);

		if (string_end) {
			string = string_end + 1;
			type = CSS_TOKEN_STRING;
		}

	} else if ((first_char == '-' && *string == '-' && string[1] == '>')
		   || (first_char == '<' && strlen(string) > 2 && !strncmp(string, "!--", 3))) {
		/* Skip SGML left and right comments */
		string += 2 + (first_char == '<');
		type = CSS_TOKEN_NONE;

	} else if (is_css_ident(first_char)) {
		scan_css(string, CSS_CHAR_IDENT);

		if (!is_css_ident_start(first_char)) {
			type = CSS_TOKEN_NAME;

		} else if (*string == '(') {
			if (string - token->string == 3
			    && !memcmp(token->string, "rgb", 3)) {
				type = CSS_TOKEN_RGB;

			} else {
				unsigned char *function_end = string + 1;

				skip_css(function_end, ')');

				/* Try to skip to the end so we do not generate
				 * tokens for every argument. */
				if (*function_end == ')') {
					type = CSS_TOKEN_FUNCTION;
					string = function_end;
				}
			}

			string++;

		} else {
			type = CSS_TOKEN_IDENT;
		}

	} else if (first_char == '<' && *string == '/') {
		/* Some kind of SGML tag end ... better bail out screaming */
		type = CSS_TOKEN_NONE;
		string = NULL;

	} else if (first_char == '/' && *string == '*') {
		/* Comments */
		type = CSS_TOKEN_NONE;

		for (string++; *string; string++)
			if (*string == '*' && string[1] == '/') {
				string += 2;
				break;
			}

	} else {
		/* TODO: Better composing of error tokens. For now we just
		 * split them down into char tokens */
	}

	token->type = type;
	token->length = string - token->string;
	scanner->position = string;
}

/* Fills the scanner with tokens. Already scanned tokens that has not been
 * requested remains and are moved to the start of the scanners token table. */
static void
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

	if (!scanner->position) {
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
		if (!*scanner->position) break;

		scan_css_token(scanner, token);

		if (token->type == CSS_TOKEN_NONE) {
 			current--;
			/* Did some one scream for us to end scanning? */
			if (!scanner->position) break;
		}
	}

	scanner->tokens = current;
	scanner->current = 0;
	if (scanner->position && !*scanner->position)
		scanner->position = NULL;
}


/* Scanner table accessors and mutators */

/* This macro checks that if the scanners table is full the last token skipping
 * or get_next_css_token() call made it possible to get the type of the next
 * token. */
#define check_css_scanner(scanner) \
	(scanner->tokens < CSS_SCANNER_TOKENS \
	 || scanner->current + 1 < scanner->tokens)

struct css_token *
get_css_token_(struct css_scanner *scanner)
{
	assert(scanner && check_css_scanner(scanner));

#ifdef CSS_SCANNER_DEBUG
	if (css_scanner_has_tokens(scanner)) {
		struct css_token *token = &scanner->table[scanner->current];

		errfile = scanner->file, errline = scanner->line;
		elinks_wdebug("<%s> %d %d [%s]", scanner->function, token->type,
			      token->length, token->string);
	}
#endif

	/* Make sure we do not return CSS_TOKEN_NONE tokens */
	assert(!css_scanner_has_tokens(scanner)
		|| scanner->table[scanner->current].type != CSS_TOKEN_NONE);

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
skip_css_tokens_(struct css_scanner *scanner, enum css_token_type skipto)
{
	struct css_token *token = get_css_token_(scanner);

	/* Skip tokens while handling some basic precedens of special chars
	 * so we don't skip to long. */
	while (token) {
		if (token->type == skipto
		    || !check_css_precedence(token->type, skipto))
			break;
		token = get_next_css_token_(scanner);
	}

	return (token && token->type == skipto)
		? get_next_css_token_(scanner) : NULL;
}

int
check_next_css_token(struct css_scanner *scanner, enum css_token_type type)
{
	assert(scanner && check_css_scanner(scanner));
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
