/* $Id: scanner.h,v 1.1 2004/01/28 00:57:56 jonas Exp $ */

#ifndef EL__UTIL_SCANNER_H
#define EL__UTIL_SCANNER_H

/* Define if you want a talking scanner */
/* #define SCANNER_DEBUG */

/* The {struct scanner_token} describes one scanner state. There are two kinds
 * of tokens: char and non-char tokens. Char tokens contains only one char and
 * simply have their char value as type. They are tokens having special control
 * meaning in the code, like ':', ';', '{', '}' and '*'. Non char tokens has
 * one or more chars and contain stuff like number or indentifier strings.  */
struct scanner_token {
	/* The type the token */
	int type;

	/* Some precedence value */
	int precedence;

	/* The start of the token string and the token length */
	unsigned char *string;
	int length;
};

/* The naming of these two macros is a bit odd .. we compare often with
 * "static" strings (I don't have a better word) so the macro name should
 * be short. --jonas */

/* Compare the string of @token with @string */
#define scanner_token_strlcasecmp(token, str, len) \
	((token) && !strlcasecmp((token)->string, (token)->length, str, len))

/* Also compares the token string but using a "static" string */
#define scanner_token_contains(token, str) \
	scanner_token_strlcasecmp(token, str, sizeof(str) - 1)


/* The number of tokens in the scanners token table:
 * At best it should be big enough to contain properties with space separated
 * values and function calls with up to 3 variables like rgb(). At worst it
 * should be no less than 2 in order to be able to peek at the next token in
 * the scanner. */
#define SCANNER_TOKENS 10
#define SCANNER_TABLE_SIZE (sizeof(struct scanner_token) * SCANNER_TOKENS)

/* The {struct scanner} describes the current state of the scanner. */
struct scanner {
	/* The very start of the scanned string and the position in the string
	 * where to scan next. If position is NULL it means that no more tokens
	 * can be retrieved from the string. */
	unsigned char *string, *position;

	/* Fills the scanner with tokens. Already scanned tokens which have not
	 * been requested remain and are moved to the start of the scanners
	 * token table. */
	/* Returns the current token or NULL if there are none. */
	struct scanner_token *(*scan)(struct scanner *scanner);

	/* The current token and number of scanned tokens in the table.
	 * If the number of scanned tokens is less than SCANNER_TOKENS
	 * it is because there are no more tokens in the string. */
	struct scanner_token *current;
	int tokens;

#ifdef SCANNER_DEBUG
	/* Debug info about the caller. */
	unsigned char *file;
	int line;
#endif

	/* The table continain already scanned tokens. It is maintained in
	 * order to optimize the scanning a bit and make it possible to look
	 * ahead at the next token. You should always use the accessors
	 * (defined below) for getting tokens from the scanner. */
	struct scanner_token table[SCANNER_TOKENS];
};

#define scanner_has_tokens(scanner) \
	((scanner)->tokens > 0 && (scanner)->current < (scanner)->table + (scanner)->tokens)

/* This macro checks if the current scanner state is valid. Meaning if the
 * scanners table is full the last token skipping or get_next_scanner_token()
 * call made it possible to get the type of the next token. */
#define check_scanner(scanner) \
	(scanner->tokens < SCANNER_TOKENS \
	 || scanner->current + 1 < scanner->table + scanner->tokens)


/* Scanner table accessors and mutators */

/* Checks the type of the next token */
#define check_next_scanner_token(scanner, token_type)				\
	(scanner_has_tokens(scanner)					\
	 && ((scanner)->current + 1 < (scanner)->table + (scanner)->tokens)	\
	 && (scanner)->current[1].type == (token_type))

/* Access current and next token. Getting the next token might cause
 * a rescan so any token pointers that has been stored in a local variable
 * might not be valid after the call. */
static inline struct scanner_token *
get_scanner_token(struct scanner *scanner)
{
	return scanner_has_tokens(scanner) ? (scanner)->current : NULL;
}

/* Do a scanning if we do not have also have access to next token. */
static inline struct scanner_token *
get_next_scanner_token(struct scanner *scanner)
{
	return (scanner_has_tokens(scanner)
		&& (++(scanner)->current + 1 >= (scanner)->table + (scanner)->tokens)
		? scanner->scan(scanner) : get_scanner_token(scanner));
}

/* Removes tokens from the scanner until it meets a token of the given type.
 * This token will then also be skipped. */
struct scanner_token *
skip_scanner_tokens(struct scanner *scanner, int skipto, int precedence);


struct scan_table_info {
	enum { SCAN_RANGE, SCAN_STRING, SCAN_END } type;
	union scan_table_data {
		struct { unsigned char *source; long length; } string;
		struct { long start, end; } range;
	} data;
	int bits;
};

#define	SCAN_TABLE_SIZE	256

/* FIXME: We assume that sizeof(void *) == sizeof(long) here! --pasky */
#define SCAN_TABLE_INFO(type, data1, data2, bits) \
	{ (type), { { (unsigned char *) (data1), (data2) } }, (bits) }

#define SCAN_TABLE_RANGE(from, to, bits) SCAN_TABLE_INFO(SCAN_RANGE, from, to, bits)
#define SCAN_TABLE_STRING(str, bits)	 SCAN_TABLE_INFO(SCAN_STRING, str, sizeof(str) - 1, bits)
#define SCAN_TABLE_END			 SCAN_TABLE_INFO(SCAN_END, 0, 0, 0)


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

/* Initializes the scan table */
void init_scanner_info(struct scanner_info *scanner_info);

/* Looks up the string from @ident to @end to in the @mappings table */
int
map_scanner_string(struct scanner_string_mapping *mappings,
		   unsigned char *ident, unsigned char *end, int base_type);

#endif
