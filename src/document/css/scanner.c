/* CSS token scanner utilities */
/* $Id: scanner.c,v 1.71 2004/01/21 05:20:58 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
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
	CSS_CHAR_IDENT		= (1 << 3),
	CSS_CHAR_IDENT_START	= (1 << 4),
	CSS_CHAR_NEWLINE	= (1 << 5),
	CSS_CHAR_NON_ASCII	= (1 << 6),
	CSS_CHAR_TOKEN		= (1 << 7),
	CSS_CHAR_WHITESPACE	= (1 << 8),
};

#define	check_css_table(c, bit)	(css_scan_table[(c)] & (bit))
#define	scan_css(s, bit)	while (check_css_table(*(s), bit)) (s)++;

#define	is_css_ident_start(c)	check_css_table(c, CSS_CHAR_IDENT_START)
#define	is_css_ident(c)		check_css_table(c, CSS_CHAR_IDENT)
#define	is_css_digit(c)		check_css_table(c, CSS_CHAR_DIGIT)
#define	is_css_hexdigit(c)	check_css_table(c, CSS_CHAR_HEX_DIGIT)
#define	is_css_char_token(c)	check_css_table(c, CSS_CHAR_TOKEN)

struct css_identifier {
	unsigned char *name;
	enum css_token_type type;
	enum css_token_type base_type;
};

static enum css_token_type
get_css_identifier_type(unsigned char *ident, int length,
			enum css_token_type base_type)
{
	static struct css_identifier identifiers2type[] = {
		{ "Hz",		CSS_TOKEN_FREQUENCY,	CSS_TOKEN_DIMENSION },
		{ "cm",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
		{ "deg",	CSS_TOKEN_ANGLE,	CSS_TOKEN_DIMENSION },
		{ "em",		CSS_TOKEN_EM,		CSS_TOKEN_DIMENSION },
		{ "ex",		CSS_TOKEN_EX,		CSS_TOKEN_DIMENSION },
		{ "grad",	CSS_TOKEN_ANGLE,	CSS_TOKEN_DIMENSION },
		{ "in",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
		{ "kHz",	CSS_TOKEN_FREQUENCY,	CSS_TOKEN_DIMENSION },
		{ "mm",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
		{ "mm",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
		{ "ms",		CSS_TOKEN_TIME,		CSS_TOKEN_DIMENSION },
		{ "pc",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
		{ "pt",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
		{ "px",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
		{ "rad",	CSS_TOKEN_ANGLE,	CSS_TOKEN_DIMENSION },
		{ "s",		CSS_TOKEN_TIME,		CSS_TOKEN_DIMENSION },

		{ "rgb",	CSS_TOKEN_RGB,		CSS_TOKEN_FUNCTION },

		{ "charset",	CSS_TOKEN_CHARSET,	CSS_TOKEN_AT_KEYWORD },
		{ "font-face",	CSS_TOKEN_FONT_FACE,	CSS_TOKEN_AT_KEYWORD },
		{ "import",	CSS_TOKEN_IMPORT,	CSS_TOKEN_AT_KEYWORD },
		{ "media",	CSS_TOKEN_MEDIA,	CSS_TOKEN_AT_KEYWORD },
		{ "page",	CSS_TOKEN_PAGE,		CSS_TOKEN_AT_KEYWORD },

		{ NULL, CSS_TOKEN_NONE, CSS_TOKEN_NONE },
	};
	struct css_identifier *ident2type = identifiers2type;

	for (; ident2type->name; ident2type++) {
		if (ident2type->base_type == base_type
		    && !strncasecmp(ident2type->name, ident, length))
			return ident2type->type;
	}

	return base_type;
}


/* This macro checks that if the scanners table is full the last token skipping
 * or get_next_css_token() call made it possible to get the type of the next
 * token. */
#define check_css_scanner(scanner) \
	(scanner->tokens < CSS_SCANNER_TOKENS \
	 || scanner->current + 1 < scanner->table + scanner->tokens)

/* Check wh�ther it is safe to skip the char @c when looking for @skipto */
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
			type = get_css_identifier_type(ident, string - ident,
							CSS_TOKEN_DIMENSION);
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
			unsigned char *ident = string;

			/* Scan both ident start and ident */
			scan_css(string, CSS_CHAR_IDENT);
			type = get_css_identifier_type(ident, string - ident,
						       CSS_TOKEN_AT_KEYWORD);
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

	} else if (is_css_ident_start(first_char)) {
		scan_css(string, CSS_CHAR_IDENT);

		if (*string == '(') {
			unsigned char *function_end = string + 1;

			/* Make sure that we have an ending ')' */
			skip_css(function_end, ')');
			if (*function_end == ')') {
				int length = string - token->string;

				type = get_css_identifier_type(token->string,
						length, CSS_TOKEN_FUNCTION);

				/* If it is not a known function just skip the
				 * how arg stuff so we don't end up generating
				 * a lot of useless tokens. */
				if (type == CSS_TOKEN_FUNCTION) {
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
struct css_token *
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

#ifdef CSS_SCANNER_DEBUG
void
dump_css_scanner(struct css_scanner *scanner)
{
	unsigned char buffer[MAX_STR_LEN];
	struct css_token *token = scanner->current;
	struct css_token *table_end = scanner->table + scanner->tokens;
	unsigned char *srcpos = token->string, *bufpos = buffer;
	int src_lookahead = 50;
	int token_lookahead = 4;
	int srclen;

	if (!css_scanner_has_tokens(scanner)) return;

	memset(buffer, 0, MAX_STR_LEN);
	for (; token_lookahead > 0 && token < table_end; token++, token_lookahead--) {
		int buflen = MAX_STR_LEN - (bufpos - buffer);
		int added = snprintf(bufpos, buflen, "[%.*s] ", token->length, token->string);

		bufpos += added;
	}

	if (scanner->tokens > token_lookahead) {
		memcpy(bufpos, "... ", 4);
		bufpos += 4;
	}

	srclen = strlen(srcpos);
	int_upper_bound(&src_lookahead, srclen);
	*bufpos++ = '[';

	/* Compress the lookahead string */
	for (; src_lookahead > 0; src_lookahead--, srcpos++, bufpos++) {
		if (*srcpos == '\n' || *srcpos == '\r' || *srcpos == '\t') {
			*bufpos++ = '\\';
			*bufpos = *srcpos == '\n' ? 'n'
				: (*srcpos == '\r' ? 'r' : 't');
		} else {
			*bufpos = *srcpos;
		}
	}

	if (srclen > src_lookahead)
		memcpy(bufpos, "...]", 4);
	else
		memcpy(bufpos, "]", 2);

	errfile = scanner->file, errline = scanner->line;
	elinks_wdebug("%s", buffer);
}

struct css_token *
get_css_token_debug(struct css_scanner *scanner)
{
	if (!css_scanner_has_tokens(scanner)) return NULL;

	dump_css_scanner(scanner);

	/* Make sure we do not return CSS_TOKEN_NONE tokens */
	assert(!css_scanner_has_tokens(scanner)
		|| scanner->current->type != CSS_TOKEN_NONE);

	return get_css_token_(scanner);
}

#endif

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
	SCAN_TABLE_RANGE('0', '9', CSS_CHAR_DIGIT | CSS_CHAR_HEX_DIGIT | CSS_CHAR_IDENT),
	SCAN_TABLE_RANGE('A', 'F', CSS_CHAR_HEX_DIGIT),
	SCAN_TABLE_RANGE('A', 'Z', CSS_CHAR_ALPHA | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),
	SCAN_TABLE_RANGE('a', 'f', CSS_CHAR_HEX_DIGIT),
	SCAN_TABLE_RANGE('a', 'z', CSS_CHAR_ALPHA | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),
	SCAN_TABLE_RANGE(161, 255, CSS_CHAR_NON_ASCII | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),

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
