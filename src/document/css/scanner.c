/* CSS token scanner utilities */
/* $Id: scanner.c,v 1.60 2004/01/21 00:38:25 jonas Exp $ */

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


/* This macro checks that if the scanners table is full the last token skipping
 * or get_next_css_token() call made it possible to get the type of the next
 * token. */
#define check_css_scanner(scanner) \
	(scanner->tokens < CSS_SCANNER_TOKENS \
	 || scanner->current + 1 < scanner->table + scanner->tokens)

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

#define CSS_SCANNER_TABLE_SIZE (sizeof(struct css_token) * CSS_SCANNER_TOKENS)

/* Fills the scanner with tokens. Already scanned tokens that has not been
 * requested remains and are moved to the start of the scanners token table. */
/* Returns the current token or NULL if there are none. */
static struct css_token *
scan_css_tokens(struct css_scanner *scanner)
{
	struct css_token *table = scanner->table;
	struct css_token *table_end = table + scanner->tokens;
	int move_to_front = int_max(table_end - scanner->current, 0);
	struct css_token *current = move_to_front ? scanner->current : table;
	size_t moved_size = 0;

	assert(scanner->current);

#ifdef CSS_SCANNER_DEBUG
	if (scanner->tokens > 0) WDBG("Rescanning");
#endif

	/* Move any untouched tokens */
	if (move_to_front) {
		moved_size = move_to_front * sizeof(struct css_token);
		memmove(table, current, moved_size);
		current = &table[move_to_front];
	}

	/* Set all unused tokens to CSS_TOKEN_NONE */
	memset(current, 0, CSS_SCANNER_TABLE_SIZE - moved_size);

	if (!scanner->position) {
		scanner->tokens = move_to_front ? move_to_front : -1;
		scanner->current = table;
		assert(check_css_scanner(scanner));
		return move_to_front ? table : NULL;
	}

	/* Scan tokens til we have filled the table */
	for (table_end = table + CSS_SCANNER_TOKENS;
	     current < table_end && *scanner->position;
	     current++) {
		scan_css(scanner->position, CSS_CHAR_WHITESPACE);
		if (!*scanner->position) break;

		scan_css_token(scanner, current);

		if (current->type == CSS_TOKEN_NONE) {
 			current--;
			/* Did some one scream for us to end scanning? */
			if (!scanner->position) break;
		}
	}

	scanner->tokens = (current - table);
	scanner->current = table;
	if (scanner->position && !*scanner->position)
		scanner->position = NULL;

	assert(check_css_scanner(scanner));
	return table;
}


/* Scanner table accessors and mutators */

struct css_token *
get_css_token_(struct css_scanner *scanner)
{
#ifdef CSS_SCANNER_DEBUG
	if (css_scanner_has_tokens(scanner)) {
		struct css_token *token = scanner->current;

		errfile = scanner->file, errline = scanner->line;
		elinks_wdebug("<%s> %d %d [%s]", scanner->function, token->type,
			      token->length, token->string);
	}
#endif

	/* Make sure we do not return CSS_TOKEN_NONE tokens */
	assert(!css_scanner_has_tokens(scanner)
		|| scanner->current->type != CSS_TOKEN_NONE);

	return css_scanner_has_tokens(scanner)
		? scanner->current : NULL;
}

struct css_token *
get_next_css_token_(struct css_scanner *scanner)
{
	scanner->current++;

	/* Do a scanning if we do not have also have access to next token */
	return (scanner->current + 1 >= scanner->table + scanner->tokens)
		? scan_css_tokens(scanner) : get_css_token_(scanner);
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


/* Initializers */

struct scan_table_info {
	enum { SCAN_RANGE, SCAN_STRING, SCAN_END } type;
	union scan_table_data {
		struct { unsigned char *source; int align; } string;
		struct { int start, end; } range;
	} data;
	int bits;
};

#define SCAN_TABLE_INFO(type, data1, data2, bits) \
	{ (type), { { (unsigned char *) (data1), (data2) } }, (bits) }

#define SCAN_TABLE_RANGE(from, to, bits) SCAN_TABLE_INFO(SCAN_RANGE, from, to, bits)
#define SCAN_TABLE_STRING(str, bits)	 SCAN_TABLE_INFO(SCAN_STRING, str, 0, bits)
#define SCAN_TABLE_END			 SCAN_TABLE_INFO(SCAN_END, 0, 0, 0)

static struct scan_table_info css_scan_table_info[] = {
	SCAN_TABLE_RANGE(161, 255, CSS_CHAR_NON_ASCII | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),
	SCAN_TABLE_RANGE('0', '9', CSS_CHAR_DIGIT | CSS_CHAR_HEX_DIGIT | CSS_CHAR_IDENT),
	SCAN_TABLE_RANGE('a', 'z', CSS_CHAR_ALPHA | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),
	SCAN_TABLE_RANGE('A', 'Z', CSS_CHAR_ALPHA | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),
	SCAN_TABLE_RANGE('a', 'f', CSS_CHAR_HEX_DIGIT),
	SCAN_TABLE_RANGE('A', 'F', CSS_CHAR_HEX_DIGIT),

	SCAN_TABLE_STRING(" \f\n\r\t\v", CSS_CHAR_WHITESPACE),
	SCAN_TABLE_STRING("\f\n\r",	 CSS_CHAR_NEWLINE),
	SCAN_TABLE_STRING("-",		 CSS_CHAR_IDENT),
	/* Unicode escape (that we do not handle yet) + other special chars */
	SCAN_TABLE_STRING("\\_",	 CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),
	/* This should contain mostly used char tokens like ':' and maybe a few
	 * garbage chars that people might put in their CSS code */
	SCAN_TABLE_STRING("({});:,",	 CSS_CHAR_TOKEN),

	SCAN_TABLE_END,
};

/* Initiate bitmaps */
static void
init_css_scan_table(void)
{
	struct scan_table_info *info = css_scan_table_info;
	int i;

	for (i = 0; info[i].type != SCAN_END; i++) {
		union scan_table_data *data = &info[i].data;

		if (info[i].type == SCAN_RANGE) {
			int index = data->range.start;

			assert(index <= data->range.end);

			for (; index <= data->range.end; index++)
				css_scan_table[index] |= info[i].bits;

		} else {
			unsigned char *string = info[i].data.string.source;

			assert(info[i].type == SCAN_STRING);

			for (; *string; string++)
				css_scan_table[*string] |= info[i].bits;
		}
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
	scanner->current = scanner->table;
	scan_css_tokens(scanner);
}
