/* CSS scanner utilities */
/* $Id: scanner.c,v 1.4 2003/06/08 12:29:31 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elusive/parser/css/scanner.h"
#include "elusive/parser/css/state.h"
#include "util/error.h"

#ifdef CSS_DEBUG
#include "elusive/parser/css/test.h"
#endif

/* This file contains functions for scanning the various tokens. Below is
 * the tokens/patterns from the flex scanner in the CSS 2 specification:
 *
 * w			[ \t\r\n\f]*
 * nl			\n|\r\n|\r|\f
 * hex			[0-9a-f]
 * num			[0-9]+|[0-9]*"."[0-9]+
 * nonascii		[\200-\377]
 *
 * unicode		\\{hex}{1,6}[ \t\r\n\f]?
 * escape		{unicode}|\\[ -~\200-\377]
 *
 * string1		\"([\t !#$%&(-~]|\\{nl}|\'|{nonascii}|{escape})*\"
 * string2		\'([\t !#$%&(-~]|\\{nl}|\"|{nonascii}|{escape})*\'
 * string		{string1}|{string2}
 *
 * nmstart		[a-z_]|{nonascii}|{escape}
 * nmchar		[a-z0-9_-]|{nonascii}|{escape}
 * ident		{nmstart}{nmchar}*
 * name			{nmchar}+
 *
 * url			([!#$%&*-~]|{nonascii}|{escape})*
 * range		\?{1,6}|{h}(\?{0,5}|{h}(\?{0,4}|{h}(\?{0,3}|{h}(\?{0,2}|{h}(\??|{h})))))
 *
 * Note about the character notation:
 *
 * <zas> \256 = 174 , \377 = 255
 * <zas> \nnn is octal notation
 */

#define	SCAN_TABLE_SIZE	256

int css_scan_table[SCAN_TABLE_SIZE];

/* Initiate bitmaps */
void
css_init_scan_table(void)
{
	int index;

	/* Initialize */
	for (index = 0; index < SCAN_TABLE_SIZE; index++)
		css_scan_table[index] = 0;

	css_scan_table['\\'] = IS_IDENT_START;
	css_scan_table['_'] |= IS_IDENT_START;
	css_scan_table['-'] |= IS_IDENT;

	/* Whitespace chars */
	css_scan_table[' ']  |= IS_WHITESPACE;		    /* space */
	css_scan_table['\t'] |= IS_WHITESPACE;		    /* horizontal tab */
	css_scan_table['\v'] |= IS_WHITESPACE;		    /* vertical tab */
	css_scan_table['\r'] |= IS_WHITESPACE | IS_NEWLINE; /* carriage return */
	css_scan_table['\n'] |= IS_WHITESPACE | IS_NEWLINE; /* line feed */
	css_scan_table['\f'] |= IS_WHITESPACE | IS_NEWLINE; /* form feed */

	for (index = 161; index <= 255; index++) {
		css_scan_table[index] |= IS_NON_ASCII | IS_IDENT | IS_IDENT_START;
	}

	for (index = '0'; index <= '9'; index++) {
		css_scan_table[index] |= IS_DIGIT | IS_HEX_DIGIT | IS_IDENT;
	}

	for (index = 'A'; index <= 'Z'; index++) {
		if ((index >= 'A') && (index <= 'F')) {
			css_scan_table[index]	   |= IS_HEX_DIGIT;
			css_scan_table[index + 32] |= IS_HEX_DIGIT;
		}

		css_scan_table[index]	   |= IS_ALPHA | IS_IDENT | IS_IDENT_START;
		css_scan_table[index + 32] |= IS_ALPHA | IS_IDENT | IS_IDENT_START;
	}
}

/* Token scanners */

enum pstate_code
css_scan_ident(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	assert(!pstate->data.token.str || !pstate->data.token.len);

	/* Signal error if expected <ident> token is not found */
	if (css_len) {
		if (!(css_scan_table[*css] & IS_IDENT_START)) {
			*pstate->data.token.len = -1;
			css_state_pop(state);
			return PSTATE_CHANGE;
		}

		css++; css_len--;
	}

	while (css_len) {
		if (css_scan_table[*css] & IS_IDENT) {
			css++; css_len--;
			continue;
		}

		/* Handle escaping */
		if (*css == '\\') {
			css++; css_len--;
#if 0
			struct css_parser_state *nextstate;

			if (!css_len) return PSTATE_SUSPEND;

			/* XXX Ok this is ugly ... we are using the <extra>
			 * state variable to get the parsed and translated
			 * escape sequence */
			/* TODO Need to mem_alloc() token in syntax tree */
			nextstate = css_state_push(state, CSS_ESCAPE);
			nextstate->data.token.str = (unsigned char **)
						    &pstate->data.token.extra;
			return PSTATE_CHANGE;
#endif
		}

		/* Atleast an ident start char has been scanned so we have a
		 * token */
		*pstate->data.token.str = *src;
		*pstate->data.token.len = *len - css_len;
		*src = css, *len = css_len;

		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	/* Handle safely suspension */
	return PSTATE_SUSPEND;
}

/* TODO this is a subset of identifier scanning and could be made
 * generic and used by scan_ident(). This would require adding
 * another stack state */
enum pstate_code
css_scan_name(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	assert(!pstate->data.token.str || !pstate->data.token.len);

	while (css_len) {
		/* TODO investigate the currious '-' allowed in <name> tokens */
		if (css_scan_table[*css] & IS_IDENT) {
			css++; css_len--;
			continue;
		}

		/* Handle escaping */
		if (*css == '\\') {
			css++; css_len--;
#if 0
			int *extra;

			if (!css_len) return PSTATE_SUSPEND;

			/* XXX Ok this is ugly ... we are using the <extra>
			 * state variable to get the parsed and translated
			 * escape sequence */
			extra = &pstate->data.token.extra;
			/* TODO Need to mem_alloc() token in syntax tree */
			pstate = css_state_push(state, CSS_ESCAPE);
			pstate->data.token.len = extra;
			return PSTATE_CHANGE;
#endif
		}

		if (css_len < *len) {
			/* Name found */
			*pstate->data.token.str = *src;
			*pstate->data.token.len = *len - css_len;

			css++; css_len--;
			*src = css, *len = css_len;
			css_state_pop(state);
			return PSTATE_CHANGE;
		}

		/* Next char is not <name> and none where scanned.
		 * Signal failure. */
		*pstate->data.token.len = -1;
		*src = css, *len = css_len;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	/* Handle safely suspension */
	return PSTATE_SUSPEND;
}

/* This is a *very* tolerant string scanning modeled after mozilla's */
/* The delimiter is stored in extra member. It is required that it will
 * already have been set/parsed */
enum pstate_code
css_scan_string(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;
	unsigned char delimiter = pstate->data.token.extra;

	assert(!pstate->data.token.str || !pstate->data.token.len
		|| !pstate->data.token.extra);


	while (css_len) {
		/* Handle the ending delimeter or unescaped newlines */
		/* Newlines are not valid unescaped. Consider them as end of
		 * the string */
		if (css_scan_table[*css] & IS_NEWLINE
			|| *css == delimiter) {

			/* Omit delimiters in the token */
			*pstate->data.token.str = *src + 1;
			*pstate->data.token.len = *len - css_len - 1;

			/* Eat the delimiter or newline.
			 * The '\n' in '\r\n' don't matter */
			css++; css_len--;
			*src = css, *len = css_len;
			pstate = css_state_pop(state);
			return PSTATE_CHANGE;
		}

		/* Handle escaping */
		if (*css == '\\') {
			if (css_len < 2) return PSTATE_SUSPEND;

			css++; css_len--;

			/* Handle string delimiter escaping locally */
			if (*css == '\'' && *css == '\"') {
				css++; css_len--;
				continue;
			}
#if 0
			/* TODO Save string start in src and allocate already
			 * handled token and put it in str. */
			token = &pstate->data.token.str;
			pstate = css_state_push(state, CSS_ESCAPE);
			pstate->data.token.str = token;
			return PSTATE_CHANGE;
#endif
		}

		/* Append to the string */
		css++; css_len--;
	}

	/* Handle safely suspension */
	return PSTATE_SUSPEND;
}

/* We should probably accept anything from 'url(' to ')' and just verify that
 * the url is valid afterwards. */
enum pstate_code
css_scan_url(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	bug_on(pstate->data.token.str && !pstate->data.token.len);

	/* Handle no url string */
	if (*pstate->data.token.len == -1) {
		pstate = css_state_repush(state, CSS_SKIP_UNTIL);
		pstate->data.skip.end_marker = ')';
		return PSTATE_CHANGE;
	}

	/* Handle url string */
	if (*pstate->data.token.len > 0) {
		pstate = css_state_repush(state, CSS_SKIP_UNTIL);
		pstate->data.skip.end_marker = ')';
		css++, css_len--;
		*src = css, *len = css_len;
		return PSTATE_CHANGE;
	}

	/* The url is in string form */
	if (css_len && (*css == '"' || *css == '\'')) {
		unsigned char **str = &(pstate->data.atrule.name);
		int *str_len = &(pstate->data.atrule.name_len);

		pstate = css_state_push(state, CSS_STRING);
		pstate->data.token.str = str;
		pstate->data.token.len = str_len;
		pstate->data.token.extra = *css;

		css++, css_len--;
		*src = css, *len = css_len;
		return PSTATE_CHANGE;
	}

	/* Handle urls as just character sequences */
	while (css_len) {
		if (css_scan_table[*css] & IS_WHITESPACE || *css == ')') {
			*pstate->data.token.str = css;
			*pstate->data.token.len = *len - css_len - 1;

			if (*css == ')') {
				css++, css_len--;
				css_state_pop(state);
				return PSTATE_CHANGE;
			}

			pstate = css_state_repush(state, CSS_SKIP_UNTIL);
			pstate->data.skip.end_marker = ')';
			css++, css_len--;
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		css++, css_len--;
	}

	return PSTATE_SUSPEND;
}

/* FIXME This needs a lot of work */
enum pstate_code
css_scan_escape(struct parser_state *state, unsigned char **src, int *len)
{
#if 0
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;
	/* More state variables to keep track of what is escaped
	 * http://lxr.mozilla.org/seamonkey/source/content/html/style/src/nsCSSScanner.cpp#658 */

	assert(!pstate->data.token.str);

	/* Assume the \ has been skipped already */
	while (css_len && escaped == -1) {
		/* TODO */
			escaped = 'g';
			continue;

			/* TODO: Handle escaping and Unicode stuff */
			/* Maybe have a table to look up in and to reference
			 * below */
		}

	/* Signal failure. */
	*pstate->data.token.len = -1;
		css_state_pop(state);
		*src = css, *len = css_len;
		return PSTATE_CHANGE;
	}

	if (escaped != -1) {
		*src = css; *len = css_len;
		*pstate->data.token.str = (unsigned char *) &escaped;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}
	/* Handle safely suspension by starting all over next time */
	return PSTATE_SUSPEND;
#endif
	css_state_pop(state);
	return PSTATE_CHANGE;
}

/* Unicode regexp:
 *	U\+({range}|{h}{1,6}-{h}{1,6})
 * TODO also handle range version */
enum pstate_code
css_scan_unicoderange(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;
	int hexdigits;

	assert(!pstate->data.unicoderange.from_len);

	if (!css_len || *css != 'U') {
		*pstate->data.unicoderange.from_len = -1;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	css++; css_len--;

	if (!css_len || *css != '+') {
		*pstate->data.unicoderange.from_len = -1;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	hexdigits = 0;

	while (css_len) {
		/* Eat as many hex digits as possible */
		if (css_scan_table[*css] & IS_HEX_DIGIT) {
			hexdigits++;
			css++; css_len--;
			continue;
		}

		/* Handle <from> field */
		if (css_scan_table[*css] == '-'
			&& !pstate->data.unicoderange.from_len
			&& hexdigits > 0
			&& hexdigits < 7) {

			/* Save length of from field */
			*pstate->data.unicoderange.from_len = (*len - 2) - css_len;
			hexdigits = 0;
			css++; css_len--;
			continue;
		}

		/* Handle <to> field */
		if ((css_scan_table[*css] & IS_WHITESPACE
				|| *css == ';'
				|| *css == '}')
			&& pstate->data.unicoderange.from_len
			&& hexdigits > 0
			&& hexdigits < 7) {

			/* TODO Validate ranges */
			/* <from> can be found at (*css+2) stretching
			 *	  pstate->data.unicoderange.from_len chars
			 * <to>	  can be found at (*css+2+pstate->...from_len)
			 *	  stretching hexdigits chars */

			css_state_pop(state);
			return PSTATE_CHANGE;
		}

		/* Signal failure */
		*pstate->data.unicoderange.from_len = -1;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	/* Start over next time */
	pstate->data.unicoderange.from_len = 0;
	return PSTATE_SUSPEND;
}

/* Hexcolor regexp:
 *	#({h}{3}|{h}{6})
 * TODO handle range version */
enum pstate_code
css_scan_hexcolor(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;
	int hexdigits;

	assert(!pstate->data.token.len);

	if (!css_len || *css != '#') {
		*pstate->data.token.len = -1;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	css++; css_len--;
	hexdigits = 0;

	while (css_len) {
		/* Eat as many hex digits as possible */
		if (css_scan_table[*css] & IS_HEX_DIGIT) {
			css++; css_len--;
			hexdigits++;
			continue;
		}

		/* Handle a good ending of hex digit sequence */
		if ((css_scan_table[*css] & IS_WHITESPACE
				|| *css == ';'
				|| *css == '}')
			&& (hexdigits == 3 || hexdigits == 6)) {

			/* The token includes the starting '#' */
			*pstate->data.token.str = css;
			*pstate->data.token.len = *len - css_len;

			*src = css, *len = css_len;
			css_state_pop(state);
			return PSTATE_CHANGE;
		}

		/* Signal failure */
		*pstate->data.token.len = -1;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	return PSTATE_SUSPEND;
}


/* Non-token scanners */

enum pstate_code
css_scan_whitespace(struct parser_state *state, unsigned char **src, int *len)
{
	unsigned char *css = *src;
	int css_len = *len;

	while (css_len) {
		if (css_scan_table[*css] & IS_WHITESPACE) {
			css++; css_len--;
			continue;
		}

		*src = css, *len = css_len;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	*src = css; *len = css_len;
	return PSTATE_SUSPEND;
}

enum pstate_code
css_scan_comment(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	/* Deal with the comment start if not resuming */
	if (!pstate->data.comment.inside) {
		/* If parsing of comments are required but no comment is found
		 * let the upper stack layer deal with it. */
		if (css_len && *css != '/') {
			*src = css, *len = css_len;
			pstate = css_state_pop(state);
			return PSTATE_CHANGE;
		}

		css++; css_len--;

		if (!css_len) return PSTATE_SUSPEND;

		if (*css != '*') {
			pstate = css_state_pop(state);
			return PSTATE_CHANGE;
		}

		css++; css_len--;
		pstate->data.comment.inside = 1;
	}

	while (css_len) {
		/* XXX For suspension to work when '*' has been recognized it
		 * is also required that the following char is available else
		 * '*' is left in the buffer. */
		if (*css != '*') {
			css++; css_len--;
			continue;
		}

		/* If the next char is not available suspend safely */
		if (css_len < 2) {
			*src = css; *len = css_len;
			return PSTATE_SUSPEND;
		}

		/* Safely check the next char */
		css++; css_len--;
		if (*css != '/') {
			css++; css_len--;
			continue;
		}

		/* The end of the comment has been found */
		css++; css_len--;
		*src = css; *len = css_len;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	/* The source ran out and no comment end was found */
	*src = css; *len = css_len;
	return PSTATE_SUSPEND;
}


/* Code skipping scanners. Used e.g. for recovering. */

enum pstate_code
css_scan_skip_until(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;
	int group = pstate->data.skip.group;
	unsigned char end_marker = pstate->data.skip.end_marker;

	if (end_marker) {
		while (css_len) {
			if (*css == end_marker) {
				/* Also skip the end marker */
				css++; css_len--;
				css_state_pop(state);
				*src = css; *len = css_len;
				return PSTATE_CHANGE;
			}

			css++; css_len--;
		}
	}

	if (group) {
		while (css_len) {
			/* TODO Possibility for specifying more groups */
			if (css_scan_table[*css] & group) {
				css_state_pop(state);
				*src = css; *len = css_len;
				return PSTATE_CHANGE;
			}

			css++; css_len--;
		}
	}

	*src = css; *len = css_len;
	return PSTATE_SUSPEND;
}

enum pstate_code
css_scan_skip(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;
	int nest_level = pstate->data.skip.nest_level;

	while (css_len) {
		/* Handle statement end */
		if (*css == ';' && nest_level == 0) {
			css++; css_len--;
			*src = css, *len = css_len;
			css_state_pop(state);
			return PSTATE_CHANGE;
		}

		/* Handle blocks by switching to block mode */
		if (*css == '}' && nest_level == 1) {
			/* Block end found */
			css++; css_len--;
			*src = css; *len = css_len;
			css_state_pop(state);
			return PSTATE_CHANGE;
		}

		/* Leaving nested block */
		if (*css == '}') {
			nest_level--;
			css++; css_len--;
			continue;
		}

		/* Entering nested block */
		if (*css == '{') {
			nest_level++;
			css++; css_len--;
			continue;
		}

		/* Handle comments generic so nothing is triggered if '{' or
		 * ';' is found inside a comment */
		CSS_SKIP_COMMENT(state, src, len, css, css_len);

		/* XXX ';' (ascii value 59) are not allowed in strings (although
		 * the string scanner will accept them). Problem is we don't
		 * want to mistake a ';' in a string. But letting the string
		 * scanner handle them gives several problems:
		 *
		 *	- It need initialized token pointers (minor)
		 *	- It will mess up parsing of the string "hmm\r\n"
		 *	  where the string scanner bails out (ends the string)
		 *	  seeing \r\n and the skipping scanner will mistake the
		 *	  last " for starting a new string.
		 *
		 * So people using (non valid) ';' in strings better not mess
		 * up the rest of their CSS code. ;)
		 */

		css++; css_len--;
	}

	pstate->data.skip.nest_level = nest_level;
	*src = css; *len = css_len;
	return PSTATE_SUSPEND;
}
