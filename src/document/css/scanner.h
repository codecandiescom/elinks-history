/* $Id: scanner.h,v 1.9 2004/01/18 16:27:50 jonas Exp $ */

#ifndef EL__DOCUMENT_CSS_SCANNER_H
#define EL__DOCUMENT_CSS_SCANNER_H

#include "document/css/property.h"
#include "util/error.h"


/* The {struct css_token} describes one CSS scanner state. There are two kinds
 * of tokens: char and non-char tokens. Char tokens contains only one char and
 * simply have their char value as type. They are tokens having special control
 * meaning in the CSS code, like ':', ';', '{', '}' and '*'. Non char tokens
 * contains one or more chars and contains stuff like digit or indentifier
 * string. */
struct css_token {
	/* The type the token */
	enum css_token_type {
		CSS_TOKEN_NONE,

		/* Char tokens have type (integer values) from 255 and down and
		 * non char tokens have values from 256 and up. */
		CSS_TOKEN_DIGIT = 256,
		CSS_TOKEN_HEX_COLOR,
		CSS_TOKEN_IDENTIFIER,
		CSS_TOKEN_NAME,
		CSS_TOKEN_PERCENTAGE,

		/* A special token for unrecognized tokens */
		CSS_TOKEN_GARBAGE,
	} type;

	/* The start of the token string and the token length */
	unsigned char *string;
	int length;
};

/* The number of tokens in the scanners token table:
 * At best it should be big enough to contain properties with space separated
 * values and function calls with up to 3 variables like rgb(). At worst it
 * should be no less than 2 in order to be able to peek at the next token in
 * the scanner. */
#define CSS_SCANNER_TOKENS 10

/* The {struct css_scanner} describes the current state of the CSS scanner. */
struct css_scanner {
	/* The very start of the scanned string */
	unsigned char *string;

	/* The current token and number of scanned tokens in the table.
	 * If the number of scanned tokens is less than CSS_SCANNER_TOKENS
	 * it is because there are no more tokens in the string. */
	int current, tokens;

	/* The table continain already scanned tokens. It is maintained in
	 * order to optimize the scanning a bit and make it possible to look
	 * ahead at the next token. You should always use the accessors
	 * (defined below) for getting tokens from the scanner. */
	struct css_token table[CSS_SCANNER_TOKENS];
};

/* Fills the scanner with tokens. Already scanned tokens that has not been
 * requested remains and are moved to the start of the scanners token table. */
void scan_css_tokens(struct css_scanner *scanner);

/* Define if you want a talking scanner */
/* #define CSS_SCANNER_DEBUG */

/* Scans a token in the @string and stores info about it in the given @token
 * struct. */
static inline struct css_token *
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
#define get_css_token(scanner)  get_css_token_(scanner, __FILE__, __LINE__)

static inline struct css_token *
get_next_css_token_(struct css_scanner *scanner, unsigned char *file, int line)
{
	scanner->current++;
	if (scanner->current >= scanner->tokens) {
		scan_css_tokens(scanner);
	}
	return get_css_token_(scanner, file, line);
}
#define get_next_css_token(scanner)  get_next_css_token_(scanner, __FILE__, __LINE__)

static inline struct css_token *
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

/* Checking of the next token type */
static inline int
check_next_css_token(struct css_scanner *scanner, enum css_token_type type)
{
	if (scanner->current + 1 >= scanner->tokens)
		scan_css_tokens(scanner);

	return scanner->current + 1 < scanner->tokens
		&& scanner->table[scanner->current + 1].type == type;
}

#define get_css_token_end(token) &(token)->string[(token)->length]

/* Compare the string of @token with @string */
#define css_token_contains(token, str, len) \
	((token) && !strlcasecmp((token)->string, (token)->length, str, len))

#define css_scanner_has_tokens(scanner) \
	((scanner)->tokens > 0 && (scanner)->current <= (scanner)->tokens)

/* Initializes the scanner. */
void init_css_scanner(struct css_scanner *scanner, unsigned char *string);

#endif
