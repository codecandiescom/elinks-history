/* CSS atrule parsers */
/* $Id: atrule.c,v 1.5 2003/07/07 00:12:23 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/css/atrule.h"
#include "elusive/parser/css/parser.h"
#include "elusive/parser/css/scanner.h"
#include "elusive/parser/css/state.h"
#include "elusive/parser/css/tree.h"
#include "elusive/parser/css/util.h"
#include "elusive/parser/parser.h"
#include "elusive/parser/property.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"

#ifdef CSS_DEBUG
#include <stdio.h>
#include "elusive/parser/css/test.h"
#endif

/* Atrules grammer:
 *
 * atrule:
 * 	  charset
 *	| import
 *	| media
 *	| page
 *	| fontface
 */
enum pstate_code
css_parse_atrule(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	/* Get the @-rule name */
	if (!pstate->data.atrule.name_len) {
		if (!css_len) return PSTATE_SUSPEND;

		if (*css == '@') {
			struct css_parser_state *nextstate;

			nextstate = css_state_push(state, CSS_IDENT);
			nextstate->data.token.str = &(pstate->data.atrule.name);
			nextstate->data.token.len = &(pstate->data.atrule.name_len);

			css++, css_len--;
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		} else {
			pstate = css_state_pop(state);
			return PSTATE_CHANGE;
		}
	}

	/* Handle no @-rule name */
	if (pstate->data.atrule.name_len == -1) {
		/* Handle error */
		css_state_repush(state, CSS_SKIP);
		return PSTATE_CHANGE;
	}

	/* Skip useless code */
	while (css_len) {
		CSS_SKIP_WHITESPACE(css, css_len);
		CSS_SKIP_COMMENT(state, src, len, css, css_len);
		break;
	}

	*src = css, *len = css_len;

	/* Try to find specialize handler */

	if (!strncasecmp("charset", pstate->data.atrule.name,
				    pstate->data.atrule.name_len)) {
		/* Is it valid to specify charsets ? */
		if (pstate->up
		    && pstate->up->state == CSS_STYLESHEET
		    && !pstate->up->data.stylesheet.no_charset) {
			css_state_repush(state, CSS_CHARSET);
		} else {
			css_state_repush(state, CSS_SKIP);
		}

		return PSTATE_CHANGE;
	}

	if (!strncasecmp("import", pstate->data.atrule.name,
				   pstate->data.atrule.name_len)) {
		/* Is it valid to import ? */
		if (pstate->up
		    && pstate->up->state == CSS_STYLESHEET
		    && !pstate->up->data.stylesheet.no_imports) {
			css_state_repush(state, CSS_IMPORT);
		} else {
			css_state_repush(state, CSS_SKIP);
		}

		return PSTATE_CHANGE;
	}

	if (!strncasecmp("media", pstate->data.atrule.name,
				  pstate->data.atrule.name_len)) {
		css_state_repush(state, CSS_MEDIA);
		return PSTATE_CHANGE;
	}

	if (!strncasecmp("page", pstate->data.atrule.name,
				 pstate->data.atrule.name_len)) {
		css_state_repush(state, CSS_PAGE);
		return PSTATE_CHANGE;
	}

	if (!strncasecmp("font-face", pstate->data.atrule.name,
				      pstate->data.atrule.name_len)) {
		css_state_repush(state, CSS_FONTFACE);
		return PSTATE_CHANGE;
	}

	/* Unrecognized @ rule */
#ifdef CSS_DEBUG
	print_token("unknown atrule", pstate->data.atrule.name,
			      pstate->data.atrule.name_len);
#endif
	css_state_repush(state, CSS_SKIP);
	return PSTATE_CHANGE;
}

/* Charset grammer:
 *
 * charset:
 * 	  '@charset' <ws> <string> <ws> ';'
 *
 * XXX Checking of wether charsets are valid to declare are checked by
 * css_parse_atrule(). And it will skip the charset if this is not the case.
 */
enum pstate_code
css_parse_charset(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	if (!css_len) return PSTATE_SUSPEND;

	/* Did we already get the charset string */
	if (!pstate->data.charset.len && (*css == '\'' || *css == '"')) {
		struct css_parser_state *nextstate;

		nextstate = css_state_push(state, CSS_STRING);
		nextstate->data.token.str = &(pstate->data.charset.str);
		nextstate->data.token.len = &(pstate->data.charset.len);
		nextstate->data.token.extra = *css;

		css++; css_len--;
		*src = css; *len = css_len;
		return PSTATE_CHANGE;
	}

	if (pstate->data.charset.len > 0) {
		struct stylesheet *stylesheet = state->real_root;

		/* Add charset to stylesheet */
		stylesheet->charset = pstate->data.charset.str;
		stylesheet->charset_len = pstate->data.charset.len;

		/* TODO Change encoding */

#ifdef CSS_DEBUG
		print_token("charset string", pstate->data.charset.str,
					      pstate->data.charset.len);
#endif

		/* 'Fall through' and surrender to the skipper */
	}

	/* Both handle error when no string and also skip to next ';' */
	css_state_repush(state, CSS_SKIP);
	return PSTATE_CHANGE;
}

/* Import grammer:
 *
 * import:
 *	  '@import' <string> media_types ';'
 *	| '@import' <uri> media_types ';'
 *
 * XXX Checking of wether imports are valid to declare are checked by
 * parse_atrule(). And it will skip the import if this is not the case.
 */
enum pstate_code
css_parse_import(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;

	/* Get the import string/uri. */
	if (!pstate->data.import.url_len) {
		struct css_parser_state *nextstate;
		unsigned char *css = *src;
		int css_len = *len;

		if (!css_len) return PSTATE_SUSPEND;

		if (*css == '\'' || *css == '"') {
			struct css_parser_state *nextstate;

			nextstate = css_state_push(state, CSS_STRING);
			nextstate->data.token.extra = *css;

			css++; css_len--;
			*src = css; *len = css_len;
			return PSTATE_CHANGE;
		} else if (*css == 'u') {
			/* Assume the location is specified by an url() */
			nextstate = css_state_push(state, CSS_FUNCTION);
		} else {
			/* Handle error */
			css_state_repush(state, CSS_SKIP);
			return PSTATE_CHANGE;
		}

		nextstate->data.token.str = &(pstate->data.import.url);
		nextstate->data.token.len = &(pstate->data.import.url_len);
		return PSTATE_CHANGE;
	}

	if (pstate->data.import.url_len == -1) {
		/* Handle error */
		css_state_repush(state, CSS_SKIP);
		return PSTATE_CHANGE;
	}

	/* Get media types if any */
	if (!pstate->data.import.matched) {
		int *matched = &(pstate->data.import.matched);

		pstate = css_state_push(state, CSS_MEDIATYPES);
		pstate->data.mediatypes.matched = matched;
		return PSTATE_CHANGE;
	}

	/* Perform the import if the mediatypes matched */
	if (pstate->data.import.matched > 0) {
		struct stylesheet *stylesheet;
		unsigned char *url = pstate->data.import.url;
		int url_len = pstate->data.import.url_len;

		/* Add import to stylesheet */
		stylesheet = state->real_root;
		add_property(&stylesheet->imports, "import", 6, url, url_len);

#ifdef CSS_DEBUG
		print_token("import url", pstate->data.import.url,
					  pstate->data.import.url_len);
#endif

		/* TODO Make import */
	}

	/* We have to get rid of whitespace and ';'. Let the skipping begin */
	css_state_repush(state, CSS_SKIP);
	return PSTATE_CHANGE;	
}

/* Media grammer:
 *
 * media:
 *	  '@media' media_types '{' ruleset* '}'
 */
enum pstate_code
css_parse_media(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	/* Get media types */
	if (!pstate->data.media.matched) {
		int *matched = &(pstate->data.media.matched);

		pstate = css_state_push(state, CSS_MEDIATYPES);
		pstate->data.mediatypes.matched = matched;
		return PSTATE_CHANGE;
	}

	/* Was there any valid media types ? */
	if (pstate->data.media.matched < 0) {
		css_state_repush(state, CSS_SKIP);
		return PSTATE_CHANGE;
	}

	/* Enter the block */
	if (!pstate->data.media.inside_block) {
		while (css_len) {
			CSS_SKIP_WHITESPACE(css, css_len);
			CSS_SKIP_COMMENT(state, src, len, css, css_len);

			/* Handle the starting of the rulesets block */
			if (*css == '{') {
				css++; css_len--;
				pstate->data.media.inside_block = 1;
				break;
			}

			/* Error recovery. */
			*src = css, *len = css_len;
			css_state_repush(state, CSS_SKIP);
			return PSTATE_CHANGE;
		}
	}

	while (css_len) {
		CSS_SKIP_WHITESPACE(css, css_len);
		CSS_SKIP_COMMENT(state, src, len, css, css_len);

		/* Check if there's a selector start */
		if (css_scan_table[*css] & IS_IDENT_START
			|| *css == '*'
			|| *css == ':'
			|| *css == '#'
			|| *css == '['
			|| *css == '.') {

			*src = css, *len = css_len;
			css_state_push(state, CSS_RULESET);
			return PSTATE_CHANGE;
		}

		/* We're done */
		if (*css == '}') {
			css++; css_len--;
			*src = css, *len = css_len;
			css_state_pop(state);
			return PSTATE_CHANGE;
		}

		/* Error recovery. XXX Assume inside the block :( */
		pstate = css_state_repush(state, CSS_SKIP);
		pstate->data.skip.nest_level = 1;
		return PSTATE_CHANGE;
	}

	*src = css, *len = css_len;
	return PSTATE_SUSPEND;
}

/* Mediatypes grammer:
 *
 * media_types:
 *	  <empty>
 *	| <ident>
 *	| media_types ',' <ident>
 */
enum pstate_code
css_parse_mediatypes(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	assert(pstate->data.mediatypes.matched);

	/* Handle error getting mediatype token */
	if (pstate->data.mediatypes.name_len < 0) {
		/* Bail out completely */
		*pstate->data.mediatypes.matched = -1;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	/* Check if a mediatype has been parsed */
	if (!*pstate->data.mediatypes.matched
		&& pstate->data.mediatypes.name_len > 0) {
		struct stylesheet *stylesheet = state->real_root;
		int mediatypes = stylesheet->mediatypes;

#define match_type(string, bitmap_entry) \
	( \
		!strncasecmp(string, \
			pstate->data.mediatypes.name, \
			pstate->data.mediatypes.name_len) \
		&& (mediatypes & bitmap_entry) \
	)

		if (!strncasecmp("all", pstate->data.mediatypes.name,
					pstate->data.mediatypes.name_len)
			|| match_type("aural", MEDIATYPE_AURAL)
			|| match_type("braille", MEDIATYPE_BRAILLE)
			|| match_type("embossed", MEDIATYPE_EMBOSSED)
			|| match_type("handheld", MEDIATYPE_HANDHELD)
			|| match_type("print", MEDIATYPE_PRINT)
			|| match_type("projection", MEDIATYPE_PROJECTION)
			|| match_type("screen", MEDIATYPE_SCREEN)
			|| match_type("tty", MEDIATYPE_TTY)
			|| match_type("tv", MEDIATYPE_TV)) {

			/* We found a match */
			*pstate->data.mediatypes.matched = 1;
		}

#undef match_type


		/* Clear this mediatype from the state */
		pstate->data.mediatypes.name_len = 0;
	}

	while (css_len) {
		CSS_SKIP_WHITESPACE(css, css_len);
		CSS_SKIP_COMMENT(state, src, len, css, css_len);

		/* XXX Here we are being a bit sloppish. Mediatypes are separated
		 * by a ','. Here we accept them with out much hesitation */
		if (*css == ',') {
			css++; css_len--;
			continue;
		}

		/* Get next mediatype */
		if (css_scan_table[*css] & IS_IDENT_START) {
			struct css_parser_state *nextstate;

			*src = css, *len = css_len;
			nextstate = css_state_push(state, CSS_IDENT);
			nextstate->data.token.str = &(pstate->data.mediatypes.name);
			nextstate->data.token.len = &(pstate->data.mediatypes.name_len);
			return PSTATE_CHANGE;
		}

		/* No more media types. Check if any were matched */
		if (!*pstate->data.mediatypes.matched) {
			/* Signal that no mediatypes were matched. */
			*pstate->data.mediatypes.matched = -1;
		}

		*src = css, *len = css_len;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	*src = css, *len = css_len;
	return PSTATE_SUSPEND;
}

/* Page grammer:
 *
 * page:
 *	 '@page' <ident>? [':' <ident>]? '{' declarations '}'
 */
enum pstate_code
css_parse_page(struct parser_state *state, unsigned char **src, int *len)
{
	css_state_repush(state, CSS_SKIP);
	return PSTATE_CHANGE;
}

/* Font-face grammer:
 *
 * font_face:
 *	 '@font-face' '{' declarations '}'
 */
enum pstate_code
css_parse_fontface(struct parser_state *state, unsigned char **src, int *len)
{
	css_state_repush(state, CSS_SKIP);
	return PSTATE_CHANGE;
}
