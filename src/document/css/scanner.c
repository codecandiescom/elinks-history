/* CSS token scanner utilities */
/* $Id: scanner.c,v 1.112 2004/01/28 00:15:48 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "document/css/scanner.h"
#include "util/error.h"
#include "util/string.h"


/* Generic scanner stuff */

#define	SCAN_TABLE_SIZE	256

struct scan_table_info {
	enum { SCAN_RANGE, SCAN_STRING, SCAN_END } type;
	union scan_table_data {
		struct { unsigned char *source; long length; } string;
		struct { long start, end; } range;
	} data;
	int bits;
};

struct scanner_string_mapping {
	unsigned char *name;
	int type;
	int base_type;
};

struct scanner_info {
	/* Table containing how to map strings to token types */
	struct scanner_string_mapping *string_mappings;

	/* Information for how to initialize the scanner table */
	struct scan_table_info *scan_table_info;

	/* The scanner table */
	/* Contains bitmaps for the various characters groups.
	 * Idea sync'ed from mozilla browser. */
	int scan_table[SCAN_TABLE_SIZE];
};

/* FIXME: We assume that sizeof(void *) == sizeof(long) here! --pasky */
#define SCAN_TABLE_INFO(type, data1, data2, bits) \
	{ (type), { { (unsigned char *) (data1), (data2) } }, (bits) }

#define SCAN_TABLE_RANGE(from, to, bits) SCAN_TABLE_INFO(SCAN_RANGE, from, to, bits)
#define SCAN_TABLE_STRING(str, bits)	 SCAN_TABLE_INFO(SCAN_STRING, str, sizeof(str) - 1, bits)
#define SCAN_TABLE_END			 SCAN_TABLE_INFO(SCAN_END, 0, 0, 0)

/* Initiate bitmaps */
static void
init_scanner_info(struct scanner_info *scanner_info)
{
	struct scan_table_info *info = scanner_info->scan_table_info;
	int *scan_table = scanner_info->scan_table;
	int i;

	for (i = 0; info[i].type != SCAN_END; i++) {
		union scan_table_data *data = &info[i].data;

		if (info[i].type == SCAN_RANGE) {
			int index = data->range.start;

			assert(data->range.start > 0);
			assert(data->range.end < SCAN_TABLE_SIZE);
			assert(data->range.start <= data->range.end);

			for (; index <= data->range.end; index++)
				scan_table[index] |= info[i].bits;

		} else {
			unsigned char *string = info[i].data.string.source;
			int pos = info[i].data.string.length - 1;

			assert(info[i].type == SCAN_STRING && pos >= 0);

			for (; pos >= 0; pos--)
				scan_table[string[pos]] |= info[i].bits;
		}
	}
}

static int
map_scanner_string(struct scanner_string_mapping *mappings,
		   unsigned char *ident, unsigned char *end, int base_type)
{
	int length = end - ident;

	for (; mappings->name; mappings++) {
		if (mappings->base_type == base_type
		    && !strlcasecmp(mappings->name, -1, ident, length))
			return mappings->type;
	}

	return base_type;
}



/* Bitmap entries for the CSS character groups used in the scanner table */

enum css_char_group {
	CSS_CHAR_ALPHA		= (1 << 0),
	CSS_CHAR_DIGIT		= (1 << 1),
	CSS_CHAR_HEX_DIGIT	= (1 << 2),
	CSS_CHAR_IDENT		= (1 << 3),
	CSS_CHAR_IDENT_START	= (1 << 4),
	CSS_CHAR_NEWLINE	= (1 << 5),
	CSS_CHAR_NON_ASCII	= (1 << 6),
	CSS_CHAR_SGML_MARKUP	= (1 << 7),
	CSS_CHAR_TOKEN		= (1 << 8),
	CSS_CHAR_TOKEN_START	= (1 << 9),
	CSS_CHAR_WHITESPACE	= (1 << 10),
};

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
	SCAN_TABLE_STRING(".#@!\"'<-/",	 CSS_CHAR_TOKEN_START),
	/* Unicode escape (that we do not handle yet) + other special chars */
	SCAN_TABLE_STRING("\\_",	 CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),
	/* This should contain mostly used char tokens like ':' and maybe a few
	 * garbage chars that people might put in their CSS code */
	SCAN_TABLE_STRING("({});:,*.",	 CSS_CHAR_TOKEN),
	SCAN_TABLE_STRING("<!->",	 CSS_CHAR_SGML_MARKUP),

	SCAN_TABLE_END,
};

static struct scanner_string_mapping css_string_mappings[] = {
	{ "Hz",		CSS_TOKEN_FREQUENCY,	CSS_TOKEN_DIMENSION },
	{ "cm",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
	{ "deg",	CSS_TOKEN_ANGLE,	CSS_TOKEN_DIMENSION },
	{ "em",		CSS_TOKEN_EM,		CSS_TOKEN_DIMENSION },
	{ "ex",		CSS_TOKEN_EX,		CSS_TOKEN_DIMENSION },
	{ "grad",	CSS_TOKEN_ANGLE,	CSS_TOKEN_DIMENSION },
	{ "in",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
	{ "kHz",	CSS_TOKEN_FREQUENCY,	CSS_TOKEN_DIMENSION },
	{ "mm",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
	{ "ms",		CSS_TOKEN_TIME,		CSS_TOKEN_DIMENSION },
	{ "pc",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
	{ "pt",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
	{ "px",		CSS_TOKEN_LENGTH,	CSS_TOKEN_DIMENSION },
	{ "rad",	CSS_TOKEN_ANGLE,	CSS_TOKEN_DIMENSION },
	{ "s",		CSS_TOKEN_TIME,		CSS_TOKEN_DIMENSION },

	{ "rgb",	CSS_TOKEN_RGB,		CSS_TOKEN_FUNCTION },
	{ "url",	CSS_TOKEN_URL,		CSS_TOKEN_FUNCTION },

	{ "charset",	CSS_TOKEN_AT_CHARSET,	CSS_TOKEN_AT_KEYWORD },
	{ "font-face",	CSS_TOKEN_AT_FONT_FACE,	CSS_TOKEN_AT_KEYWORD },
	{ "import",	CSS_TOKEN_AT_IMPORT,	CSS_TOKEN_AT_KEYWORD },
	{ "media",	CSS_TOKEN_AT_MEDIA,	CSS_TOKEN_AT_KEYWORD },
	{ "page",	CSS_TOKEN_AT_PAGE,	CSS_TOKEN_AT_KEYWORD },

	{ NULL, CSS_TOKEN_NONE, CSS_TOKEN_NONE },
};

static struct scanner_info css_scanner_info = {
	css_string_mappings,
	css_scan_table_info,
};

#define	check_css_table(c, bit)	(css_scanner_info.scan_table[(c)] & (bit))
#define	scan_css(s, bit)	while (check_css_table(*(s), bit)) (s)++;
#define	scan_back_css(s, bit)	while (check_css_table(*(s), bit)) (s)--;

#define	is_css_ident_start(c)	check_css_table(c, CSS_CHAR_IDENT_START)
#define	is_css_ident(c)		check_css_table(c, CSS_CHAR_IDENT)
#define	is_css_digit(c)		check_css_table(c, CSS_CHAR_DIGIT)
#define	is_css_hexdigit(c)	check_css_table(c, CSS_CHAR_HEX_DIGIT)
#define	is_css_char_token(c)	check_css_table(c, CSS_CHAR_TOKEN)
#define	is_css_token_start(c)	check_css_table(c, CSS_CHAR_TOKEN_START)

#define ident2type(ident, end, base_type) \
	map_scanner_string(css_string_mappings, ident, end, base_type)


/* This macro checks that if the scanners table is full the last token skipping
 * or get_next_scanner_token() call made it possible to get the type of the next
 * token. */
#define check_scanner(scanner) \
	(scanner->tokens < SCANNER_TOKENS \
	 || scanner->current + 1 < scanner->table + scanner->tokens)

#define	skip_css(s, skipto)							\
	while (*(s) && *(s) != (skipto) && check_css_precedence(*(s), skipto)) {\
		if (*(s) == '"' || *(s) == '\'') {				\
			unsigned char *end = strchr(s + 1, *(s));		\
										\
			if (end) (s) = end;					\
		}								\
		(s)++;								\
	}


static inline void
scan_css_token(struct scanner *scanner, struct scanner_token *token)
{
	unsigned char *string = scanner->position;
	unsigned char first_char = *string;
	enum css_token_type type = CSS_TOKEN_GARBAGE;
	int real_length = -1;

	assert(first_char);
	token->string = string++;

	if (is_css_char_token(first_char)) {
		type = first_char;

	} else if (is_css_digit(first_char) || first_char == '.') {
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
			if (first_char != '.')
				type = CSS_TOKEN_PERCENTAGE;
			string++;

		} else if (!is_css_ident_start(*string)) {
			type = CSS_TOKEN_NUMBER;

		} else {
			unsigned char *ident = string;

			scan_css(string, CSS_CHAR_IDENT);
			type = ident2type(ident, string, CSS_TOKEN_DIMENSION);
		}

	} else if (is_css_ident_start(first_char)) {
		scan_css(string, CSS_CHAR_IDENT);

		if (*string == '(') {
			unsigned char *function_end = string + 1;

			/* Make sure that we have an ending ')' */
			skip_css(function_end, ')');
			if (*function_end == ')') {
				type = ident2type(token->string, string,
						  CSS_TOKEN_FUNCTION);

				/* If it is not a known function just skip the
				 * how arg stuff so we don't end up generating
				 * a lot of useless tokens. */
				if (type == CSS_TOKEN_FUNCTION) {
					string = function_end;

				} else if (type == CSS_TOKEN_URL) {
					/* Extracting the URL first removes any
					 * leading or ending whitespace and
					 * then see if the url is given in a
					 * string. If that is the case the
					 * string delimiters are also trimmed.
					 * This is not totally correct because
					 * we should of course handle escape
					 * sequences .. but that will have to
					 * be fixed later.  */
					unsigned char *from = string + 1;
					unsigned char *to = function_end - 1;

					scan_css(from, CSS_CHAR_WHITESPACE);
					scan_back_css(to, CSS_CHAR_WHITESPACE);

					if (*from == '"' || *from == '\'') from++;
					if (*to == '"' || *to == '\'') to--;

					token->string = from;
					real_length = to - from + 1;
					assert(real_length >= 0);
					string = function_end;
				}

				assert(type != CSS_TOKEN_RGB || *string == '(');
				assert(type != CSS_TOKEN_URL || *string == ')');
				assert(type != CSS_TOKEN_FUNCTION || *string == ')');
			}

			string++;

		} else {
			type = CSS_TOKEN_IDENT;
		}

	} else if (!is_css_token_start(first_char)) {
		/* TODO: Better composing of error tokens. For now we just
		 * split them down into char tokens */

	} else if (first_char == '#') {
		/* Check whether it is hexcolor or hash token */
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
			type = ident2type(ident, string, CSS_TOKEN_AT_KEYWORD);
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
			/* We don't want the delimiters in the token */
			token->string++;
			real_length = string_end - token->string;
			string = string_end + 1;
			type = CSS_TOKEN_STRING;
		}

	} else if (first_char == '<' || first_char == '-') {
		/* Try to navigate SGML tagsoup */

		if (*string == '/') {
			/* Some kind of SGML tag end ... better bail out screaming */
			type = CSS_TOKEN_NONE;

		} else {
			unsigned char *sgml = string;

			/* Skip anything looking like SGML "<!--" and "-->"
			 * comments */
			scan_css(sgml, CSS_CHAR_SGML_MARKUP);

			if (sgml - string >= 2
			    && ((first_char == '<' && *string == '!')
				|| (first_char == '-' && *sgml == '>'))) {
				type = CSS_TOKEN_SKIP;
				string = sgml + 1;
			}
		}

	} else if (first_char == '/') {
		/* Comments */
		if (*string == '*') {
			type = CSS_TOKEN_SKIP;

			for (string++; *string; string++)
				if (*string == '*' && string[1] == '/') {
					string += 2;
					break;
				}
		}

	} else {
		INTERNAL("Someone forgot to put code for recognizing tokens "
			 "which start with '%c'.", first_char);
	}

	token->type = type;
	token->length = real_length > 0 ? real_length : string - token->string;
	token->precedence = get_css_precedence(type);
	scanner->position = string;
}

#define SCANNER_TABLE_SIZE (sizeof(struct scanner_token) * SCANNER_TOKENS)

static struct scanner_token *
scan_css_tokens(struct scanner *scanner)
{
	struct scanner_token *table = scanner->table;
	struct scanner_token *table_end = table + scanner->tokens;
	int move_to_front = int_max(table_end - scanner->current, 0);
	struct scanner_token *current = move_to_front ? scanner->current : table;
	size_t moved_size = 0;

	assert(scanner->current);

#ifdef SCANNER_DEBUG
	if (scanner->tokens > 0) WDBG("Rescanning");
#endif

	/* Move any untouched tokens */
	if (move_to_front) {
		moved_size = move_to_front * sizeof(struct scanner_token);
		memmove(table, current, moved_size);
		current = &table[move_to_front];
	}

	/* Set all unused tokens to CSS_TOKEN_NONE */
	memset(current, 0, SCANNER_TABLE_SIZE - moved_size);

	if (!scanner->position) {
		scanner->tokens = move_to_front ? move_to_front : -1;
		scanner->current = table;
		assert(check_scanner(scanner));
		return move_to_front ? table : NULL;
	}

	/* Scan tokens until we fill the table */
	for (table_end = table + SCANNER_TOKENS;
	     current < table_end && *scanner->position;
	     current++) {
		scan_css(scanner->position, CSS_CHAR_WHITESPACE);
		if (!*scanner->position) break;

		scan_css_token(scanner, current);

		/* Did some one scream for us to end the madness? */
		if (current->type == CSS_TOKEN_NONE) {
			scanner->position = NULL;
			current--;
			break;
		}

		/* Shall we scratch this token? */
		if (current->type == CSS_TOKEN_SKIP) {
 			current--;
		}
	}

	scanner->tokens = (current - table);
	scanner->current = table;
	if (scanner->position && !*scanner->position)
		scanner->position = NULL;

	assert(check_scanner(scanner));
	return table;
}


/* Scanner table accessors and mutators */

#ifdef SCANNER_DEBUG
void
dump_scanner(struct scanner *scanner)
{
	unsigned char buffer[MAX_STR_LEN];
	struct scanner_token *token = scanner->current;
	struct scanner_token *table_end = scanner->table + scanner->tokens;
	unsigned char *srcpos = token->string, *bufpos = buffer;
	int src_lookahead = 50;
	int token_lookahead = 4;
	int srclen;

	if (!scanner_has_tokens(scanner)) return;

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

struct scanner_token *
get_scanner_token_debug(struct scanner *scanner)
{
	if (!scanner_has_tokens(scanner)) return NULL;

	dump_scanner(scanner);

	/* Make sure we do not return invalid tokens */
	assert(!scanner_has_tokens(scanner)
		|| scanner->current->type != 0);

	return get_scanner_token(scanner);
}

#endif

struct scanner_token *
skip_scanner_tokens(struct scanner *scanner, int skipto, int precedence)
{
	struct scanner_token *token = get_scanner_token(scanner);

	/* Skip tokens while handling some basic precedens of special chars
	 * so we don't skip to long. */
	while (token) {
		if (token->type == skipto
		    || token->precedence > precedence)
			break;
		token = get_next_scanner_token(scanner);
	}

	return (token && token->type == skipto)
		? get_next_scanner_token(scanner) : NULL;
}


/* Initializers */

void
init_css_scanner(struct scanner *scanner, unsigned char *string)
{
	static int did_init_scan_table;

	if (!did_init_scan_table) {
		init_scanner_info(&css_scanner_info);
		did_init_scan_table = 1;
	}

	memset(scanner, 0, sizeof(struct scanner));

	scanner->scan = scan_css_tokens;
	scanner->string = string;
	scanner->position = string;
	scanner->current = scanner->table;
	scan_css_tokens(scanner);
}
