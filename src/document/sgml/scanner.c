/* SGML token scanner utilities */
/* $Id: scanner.c,v 1.5 2004/09/25 23:38:08 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "document/sgml/scanner.h"
#include "util/error.h"
#include "util/scanner.h"
#include "util/string.h"


/* Bitmap entries for the SGML character groups used in the scanner table */

/* The SGML tokenizer maintains a state that can be either text or element
 * state. The state has only meaning while doing the actual scanning and is not
 * accessible at the parsing time. */
enum sgml_scanner_state {
	SGML_STATE_TEXT,
	SGML_STATE_ELEMENT,
};

enum sgml_char_group {
	SGML_CHAR_ENTITY	= (1 << 1),
	SGML_CHAR_IDENT		= (1 << 2),
	SGML_CHAR_NEWLINE	= (1 << 3),
	SGML_CHAR_WHITESPACE	= (1 << 4),
	SGML_CHAR_NOT_TEXT	= (1 << 5),
	SGML_CHAR_NOT_ATTRIBUTE	= (1 << 6),
};

static struct scan_table_info sgml_scan_table_info[] = {
	SCAN_TABLE_RANGE("0", '9', SGML_CHAR_IDENT | SGML_CHAR_ENTITY),
	SCAN_TABLE_RANGE("A", 'Z', SGML_CHAR_IDENT | SGML_CHAR_ENTITY),
	SCAN_TABLE_RANGE("a", 'z', SGML_CHAR_IDENT | SGML_CHAR_ENTITY),
	/* For the octal number impared (me including) \241 is 161 --jonas */
	SCAN_TABLE_RANGE("\241", 255, SGML_CHAR_IDENT | SGML_CHAR_ENTITY),

	SCAN_TABLE_STRING("-_:.",	 SGML_CHAR_IDENT | SGML_CHAR_ENTITY),
	SCAN_TABLE_STRING("#",		 SGML_CHAR_ENTITY),
	SCAN_TABLE_STRING(" \f\n\r\t\v", SGML_CHAR_WHITESPACE),
	SCAN_TABLE_STRING("\f\n\r",	 SGML_CHAR_NEWLINE),
	SCAN_TABLE_STRING("<&",		 SGML_CHAR_NOT_TEXT),
	SCAN_TABLE_STRING("<=>",	 SGML_CHAR_NOT_ATTRIBUTE),

	SCAN_TABLE_END,
};

static struct scanner_string_mapping sgml_string_mappings[] = {
	{ "--",		SGML_TOKEN_NOTATION_COMMENT,	SGML_TOKEN_NOTATION },
	{ "ATTLIST",	SGML_TOKEN_NOTATION_ATTLIST,	SGML_TOKEN_NOTATION },
	{ "DOCTYPE",	SGML_TOKEN_NOTATION_DOCTYPE,	SGML_TOKEN_NOTATION },
	{ "ELEMENT",	SGML_TOKEN_NOTATION_ELEMENT,	SGML_TOKEN_NOTATION },
	{ "ENTITY",	SGML_TOKEN_NOTATION_ENTITY,	SGML_TOKEN_NOTATION },

	{ "xml",	SGML_TOKEN_PROCESS_XML,		SGML_TOKEN_PROCESS },

	{ NULL, SGML_TOKEN_NONE, SGML_TOKEN_NONE },
};

static struct scanner_token *scan_sgml_tokens(struct scanner *scanner);

struct scanner_info sgml_scanner_info = {
	sgml_string_mappings,
	sgml_scan_table_info,
	scan_sgml_tokens,
};

#define	check_sgml_table(c, bit)	(sgml_scanner_info.scan_table[(c)] & (bit))

#define	scan_sgml(scanner, s, bit)					\
	while ((s) < (scanner)->end && check_sgml_table(*(s), bit)) (s)++;

#define	is_sgml_ident(c)	check_sgml_table(c, SGML_CHAR_IDENT)
#define	is_sgml_entity(c)	check_sgml_table(c, SGML_CHAR_ENTITY)
#define	is_sgml_space(c)	check_sgml_table(c, SGML_CHAR_WHITESPACE)
#define	is_sgml_text(c)		!check_sgml_table(c, SGML_CHAR_NOT_TEXT)
#define	is_sgml_token_start(c)	check_sgml_table(c, SGML_CHAR_TOKEN_START)
#define	is_sgml_attribute(c)	!check_sgml_table(c, SGML_CHAR_NOT_ATTRIBUTE | SGML_CHAR_WHITESPACE)


/* Text token scanning */

/* I think it is faster to not check the table here --jonas */
#define foreach_sgml_cdata(scanner, str)				\
	for (; ((str) < (scanner)->end && *(str) != '<' && *(str) != '&'); (str)++)

static inline void
scan_sgml_text_token(struct scanner *scanner, struct scanner_token *token)
{
	unsigned char *string = scanner->position;
	unsigned char first_char = *string;
	enum sgml_token_type type = SGML_TOKEN_GARBAGE;
	int real_length = -1;

	/* In scan_sgml_tokens() we check that first_char != '<' */
	assert(first_char != '<' && scanner->state == SGML_STATE_TEXT);

	token->string = string++;

	if (first_char == '&') {
		if (is_sgml_entity(*string)) {
			scan_sgml(scanner, string, SGML_CHAR_ENTITY);
			type = SGML_TOKEN_ENTITY;
			token->string++;
			real_length = string - token->string;
		}

		foreach_sgml_cdata (scanner, string)
			if (*string == ';') break;

	} else {
		if (is_sgml_space(first_char)) {
			scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);
			type = string < scanner->end && is_sgml_text(*string)
			     ? SGML_TOKEN_TEXT : SGML_TOKEN_SPACE;
		} else {
			type = SGML_TOKEN_TEXT;
		}

		foreach_sgml_cdata (scanner, string)
			/* m33p */;
	}

	token->type = type;
	token->length = real_length >= 0 ? real_length : string - token->string;
	token->precedence = get_sgml_precedence(type);
	scanner->position = string;
}


/* Element scanning */

/* Check whether it is safe to skip the @token when looking for @skipto. */
static inline int
check_sgml_precedence(int type, int skipto)
{
	return get_sgml_precedence(type) <= get_sgml_precedence(skipto);
}

/* XXX: Only element or ``in tag'' precedence is handled correctly however
 * using this function for CDATA or text would be overkill. */
static inline unsigned char *
skip_sgml(struct scanner *scanner, unsigned char **string, unsigned char skipto,
	  int check_quoting)
{
	unsigned char *pos = *string;

	for (; pos < scanner->end; pos++) {
		if (*pos == skipto) {
			*string = pos + 1;
			return pos;
		}

		if (!check_sgml_precedence(*pos, skipto))
			break;

		if (check_quoting && isquote(*pos)) {
			int length = scanner->end - pos;
			unsigned char *end = memchr(pos + 1, *pos, length);

			if (end) pos = end;
		}
	}

	*string = pos;
	return NULL;
}

#define scan_sgml_attribute(scanner, str)				\
	while ((str) < (scanner)->end && is_sgml_attribute(*(str)))	\
	       (str)++;

static inline void
scan_sgml_element_token(struct scanner *scanner, struct scanner_token *token)
{
	unsigned char *string = scanner->position;
	unsigned char first_char = *string;
	enum sgml_token_type type = SGML_TOKEN_GARBAGE;
	int real_length = -1;

	token->string = string++;

	if (first_char == '<') {
		scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);

		if (scanner->state == SGML_STATE_ELEMENT) {
			/* Already inside an element so insert a tag end token
			 * and continue scanning in next iteration. */
			string--;
			real_length = 0;
			type = SGML_TOKEN_TAG_END;
			scanner->state = SGML_STATE_TEXT;

		} else if (is_sgml_ident(*string)) {
			token->string = string;
			scan_sgml(scanner, string, SGML_CHAR_IDENT);

			real_length = string - token->string;

			scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);
			if (*string == '>') {
				type = SGML_TOKEN_ELEMENT;
				string++;
			} else {
				scanner->state = SGML_STATE_ELEMENT;
				type = SGML_TOKEN_ELEMENT_BEGIN;
			}

		} else if (*string == '!') {
			unsigned char *ident;
			enum sgml_token_type base = SGML_TOKEN_NOTATION;

			string++;
			scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);
			token->string = ident = string;
			scan_sgml(scanner, string, SGML_CHAR_IDENT);

			type = map_scanner_string(scanner, ident, string, base);

			switch (type) {
			case SGML_TOKEN_NOTATION_COMMENT:
				token->string = string;

				while (skip_sgml(scanner, &string, '>', 0)) {
					unsigned char *pos = string - 3;

					if (pos < token->string
					    || pos[0] != '-' || pos[1] != '-')
						continue;

					real_length = pos - token->string;
					assert(real_length >= 0);
					break;
				}
				break;

			default:
				skip_sgml(scanner, &string, '>', 0);
			}

		} else if (*string == '?') {
			unsigned char *pos;
			enum sgml_token_type base = SGML_TOKEN_PROCESS;

			string++;
			scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);
			token->string = pos = string;
			scan_sgml(scanner, string, SGML_CHAR_IDENT);

			type = map_scanner_string(scanner, pos, string, base);

			/* Figure out where the processing instruction ends */
			for (pos = string; skip_sgml(scanner, &pos, '>', 0); ) {
				if (pos[-2] != '?') continue;

				/* Set length until '?' char and move position
				 * beyond '>'. */
				real_length = pos - token->string - 2;
				break;
			}

			switch (type) {
			case SGML_TOKEN_PROCESS_XML:
				/* We want to parse the attributes */
				assert(scanner->state != SGML_STATE_ELEMENT);
				scanner->state = SGML_STATE_ELEMENT;
				break;

			default:
				/* Just skip the whole thing */
				string = pos;
			}

		} else if (*string == '/') {
			string++;
			scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);

			if (is_sgml_ident(*string)) {
				token->string = string;
				scan_sgml(scanner, string, SGML_CHAR_IDENT);
				real_length = string - token->string;

				if (skip_sgml(scanner, &string, '>', 1))
					type = SGML_TOKEN_ELEMENT_END;

			} else if (*string == '>') {
				string++;
				real_length = 0;
				type = SGML_TOKEN_ELEMENT_END;
			}

			if (type != SGML_TOKEN_GARBAGE)
				scanner->state = SGML_STATE_TEXT;

		} else {
			/* Alien < > stuff so ignore it */
			skip_sgml(scanner, &string, '>', 0);
		}

	} else if (first_char == '=') {
		type = '=';

	} else if (first_char == '?' || first_char == '>') {
		if (first_char == '?') {
			skip_sgml(scanner, &string, '>', 0);
		}

		type = SGML_TOKEN_TAG_END;
		assert(scanner->state == SGML_STATE_ELEMENT);
		scanner->state = SGML_STATE_TEXT;
 
	} else if (first_char == '/') {
		if (*string == '>') {
			string++;
			real_length = 0;
			type = SGML_TOKEN_ELEMENT_EMPTY_END;
			assert(scanner->state == SGML_STATE_ELEMENT);
			scanner->state = SGML_STATE_TEXT;
		} else if (is_sgml_attribute(*string)) {
			scan_sgml_attribute(scanner, string);
			type = SGML_TOKEN_ATTRIBUTE;
		}

	} else if (isquote(first_char)) {
		int size = scanner->end - string;
		unsigned char *string_end = memchr(string, first_char, size);

		if (string_end) {
			/* We don't want the delimiters in the token */
			token->string++;
			real_length = string_end - token->string;
			string = string_end + 1;
			type = SGML_TOKEN_STRING;
		} else if (is_sgml_attribute(*string)) {
			token->string++;
			scan_sgml_attribute(scanner, string);
			type = SGML_TOKEN_ATTRIBUTE;
		}

	} else if (is_sgml_attribute(first_char)) {
		if (is_sgml_ident(first_char)) {
			scan_sgml(scanner, string, SGML_CHAR_IDENT);
			type = SGML_TOKEN_IDENT;
		}

		if (is_sgml_attribute(*string)) {
			scan_sgml_attribute(scanner, string);
			type = SGML_TOKEN_ATTRIBUTE;
		}
	}

	token->type = type;
	token->length = real_length >= 0 ? real_length : string - token->string;
	token->precedence = get_sgml_precedence(type);
	scanner->position = string;
}


/* Scanner multiplexor */

static struct scanner_token *
scan_sgml_tokens(struct scanner *scanner)
{
	struct scanner_token *table_end = scanner->table + SCANNER_TOKENS;
	struct scanner_token *current;

	if (!begin_token_scanning(scanner))
		return get_scanner_token(scanner);

	/* Scan tokens until we fill the table */
	for (current = scanner->table + scanner->tokens;
	     current < table_end && scanner->position < scanner->end;
	     current++) {
		if (scanner->state == SGML_STATE_ELEMENT
		    || *scanner->position == '<') {
			scan_sgml(scanner, scanner->position, SGML_CHAR_WHITESPACE);
			if (scanner->position >= scanner->end) break;

			scan_sgml_element_token(scanner, current);

			/* Shall we scratch this token? */
			if (current->type == SGML_TOKEN_SKIP) {
				current--;
			}
		} else {
			scan_sgml_text_token(scanner, current);
		}
	}

	return end_token_scanning(scanner, current);
}
