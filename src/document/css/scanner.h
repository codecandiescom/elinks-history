/* $Id: scanner.h,v 1.41 2004/01/21 04:25:34 jonas Exp $ */

#ifndef EL__DOCUMENT_CSS_SCANNER_H
#define EL__DOCUMENT_CSS_SCANNER_H

#include "document/css/property.h"
#include "util/error.h"


/* The various token types and what they contain. Patterns taken from
 * the flex scanner declarations in the CSS 2 Specification. */
enum css_token_type {
	CSS_TOKEN_NONE,

	/* Char tokens from 0-255 have their char value as type and non string
	 * tokens have values from 256 and up. */

	/* Low level string tokens: */

	/* {...} means char group, <...> means token */
	/* {identstart}	[a-z_]|{nonascii} */
	/* {ident}	[a-z0-9_-]|{nonascii} */
	/* <ident>	{identstart}{ident}* */
	/* <name>	{ident}+ */
	/* <number>	[0-9]+|[0-9]*"."[0-9]+ */

	/* Percentage is put because although it looks like being composed of
	 * <number> and '%' floating point numbers are really not allowed but
	 * strtol() will round it down for us ;) */
	CSS_TOKEN_IDENT = 256,	/* <ident> */
	CSS_TOKEN_NUMBER,	/* <number> */
	CSS_TOKEN_PERCENTAGE,	/* <number>% */
	CSS_TOKEN_STRING,	/* Char sequence delimted by matching ' or " */

	/* High level string tokens: */

	/* The various number values; dimension being the most generic */
	CSS_TOKEN_ANGLE,	/* <number>rad, <number>grad or <number>deg */
	CSS_TOKEN_DIMENSION,	/* <number><ident> */
	CSS_TOKEN_EM,		/* <number>em */
	CSS_TOKEN_EX,		/* <number>ex */
	CSS_TOKEN_FREQUENCY,	/* <number>Hz or <number>kHz */
	CSS_TOKEN_LENGTH,	/* <number>{px,cm,mm,in,pt,pc} */
	CSS_TOKEN_TIME,		/* <number>ms or <number>s */

	/* XXX: CSS_TOKEN_HASH conflicts with CSS_TOKEN_HEX_COLOR. Generating
	 * hex color tokens has precedence and the hash token user have to
	 * treat CSS_TOKEN_HASH and CSS_TOKEN_HEX_COLOR alike. */
	CSS_TOKEN_HASH,		/* #<name> */
	CSS_TOKEN_HEX_COLOR,	/* #[0-9a-f]\{3,6} */

	/* Unknown functions contain also args so parsing is easier but for
	 * known functions we want to generate tokens for every arg and arg
	 * delimiter ( ',' or ')' ). */
	CSS_TOKEN_FUNCTION,	/* <ident>(<args>) */
	CSS_TOKEN_RGB,		/* rgb( */

	/* TODO: @-rules; CSS_TOKEN_IMPORT etc. */
	CSS_TOKEN_AT_KEYWORD,	/* @<ident> */
	CSS_TOKEN_IMPORTANT,	/* !<whitespace>important */

	/* TODO: Selector stuff like "|=" and "~=" */

	/* A special token for unrecognized strings */
	CSS_TOKEN_GARBAGE,
};

/* Define if you want a talking scanner */
/* #define CSS_SCANNER_DEBUG */

/* The {struct css_token} describes one CSS scanner state. There are two kinds
 * of tokens: char and non-char tokens. Char tokens contains only one char and
 * simply have their char value as type. They are tokens having special control
 * meaning in the CSS code, like ':', ';', '{', '}' and '*'. Non char tokens
 * contains one or more chars and contains stuff like digit or indentifier
 * string. */
struct css_token {
	/* The type the token */
	enum css_token_type type;

	/* The start of the token string and the token length */
	unsigned char *string;
	int length;
};

/* The naming of these two macros is a bit odd .. we compare often with
 * "static" strings (I don't have a better word) so the macro name should
 * be short. --jonas */

/* Compare the string of @token with @string */
#define css_token_strlcasecmp(token, str, len) \
	((token) && !strlcasecmp((token)->string, (token)->length, str, len))

/* Also compares the token string but using a "static" string */
#define css_token_contains(token, str) \
	css_token_strlcasecmp(token, str, sizeof(str) - 1)


/* The number of tokens in the scanners token table:
 * At best it should be big enough to contain properties with space separated
 * values and function calls with up to 3 variables like rgb(). At worst it
 * should be no less than 2 in order to be able to peek at the next token in
 * the scanner. */
#define CSS_SCANNER_TOKENS 10

/* The {struct css_scanner} describes the current state of the CSS scanner. */
struct css_scanner {
	/* The very start of the scanned string and the position in the string
	 * where to scan next. If position is NULL it means that no more tokens
	 * can be retrieved from the string. */
	unsigned char *string, *position;

	/* The current token and number of scanned tokens in the table.
	 * If the number of scanned tokens is less than CSS_SCANNER_TOKENS
	 * it is because there are no more tokens in the string. */
	struct css_token *current;
	int tokens;

#ifdef CSS_SCANNER_DEBUG
	/* Debug info about the caller. */
	unsigned char *file;
	int line;
#endif

	/* The table continain already scanned tokens. It is maintained in
	 * order to optimize the scanning a bit and make it possible to look
	 * ahead at the next token. You should always use the accessors
	 * (defined below) for getting tokens from the scanner. */
	struct css_token table[CSS_SCANNER_TOKENS];
};


/* Initializes the scanner. */
void init_css_scanner(struct css_scanner *scanner, unsigned char *string);

#define css_scanner_has_tokens(scanner) \
	((scanner)->tokens > 0 && (scanner)->current < (scanner)->table + (scanner)->tokens)

/* Fills the scanner with tokens. Already scanned tokens that has not been
 * requested remains and are moved to the start of the scanners token table. */
/* Returns the current token or NULL if there are none. */
struct css_token *scan_css_tokens(struct css_scanner *scanner);

/* Scanner table accessors and mutators */

/* Checks the type of the next token */
#define check_next_css_token(scanner, token_type)				\
	(css_scanner_has_tokens(scanner)					\
	 && ((scanner)->current + 1 < (scanner)->table + (scanner)->tokens)	\
	 && (scanner)->current[1].type == (token_type))

/* Access current and next token. Getting the next token might cause
 * a rescan so any token pointers that has been stored in a local variable
 * might not be valid after the call. */
#define get_css_token_(scanner)							\
	(css_scanner_has_tokens(scanner) ? (scanner)->current : NULL)

/* Do a scanning if we do not have also have access to next token */
#define get_next_css_token_(scanner)						\
	(css_scanner_has_tokens(scanner)					\
	 && (++(scanner)->current + 1 >= (scanner)->table + (scanner)->tokens)	\
	 ? scan_css_tokens(scanner) : get_css_token_(scanner))

/* Removes tokens from the scanner until it meets a token of the given type.
 * This token will then also be skipped. */
struct css_token *
skip_css_tokens_(struct css_scanner *scanner, enum css_token_type type);

#ifndef CSS_SCANNER_DEBUG
#define get_css_token(scanner)		get_css_token_(scanner)
#define get_next_css_token(scanner)	get_next_css_token_(scanner)
#define skip_css_tokens(scanner, token)	skip_css_tokens_(scanner, token)
#else
struct css_token *get_css_token_debug(struct css_scanner *scanner);
#define get_css_token(s)	((s)->file = __FILE__, (s)->line = __LINE__, get_css_token_debug(s))
#define get_next_css_token(s)	((s)->file = __FILE__, (s)->line = __LINE__, get_next_css_token_(s))
#define skip_css_tokens(s, t)	((s)->file = __FILE__, (s)->line = __LINE__, skip_css_tokens_(s, t))
#endif /* CSS_SCANNER_DEBUG */

#endif
