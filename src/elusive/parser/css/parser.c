/* Parser CSS backend */
/* $Id: parser.c,v 1.3 2003/06/08 12:29:31 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/css/atrule.h"
#include "elusive/parser/css/parser.h"
#include "elusive/parser/css/ruleset.h"
#include "elusive/parser/css/scanner.h"
#include "elusive/parser/css/state.h"
#include "elusive/parser/css/tree.h"
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

/* Stylesheet grammer:
 *
 * Both @charset and @import rules need special handling:
 * 
 *  o charset rule is only valid if it's the first declaration
 *  o import rules have to precede all rule sets
 *
 * Moving them up here will happily ignore any 
 *
 * stylesheet:
 * 	  <empty>
 * 	| stylesheet atrule
 * 	| stylesheet ruleset
 */
static enum pstate_code
parse_css_stylesheet(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	while (css_len) {
		CSS_SKIP_WHITESPACE(css, css_len);
		CSS_SKIP_COMMENT(state, src, len, css, css_len);

		/* Handle @ rules */
		if (*css == '@') {
			pstate = css_state_push(state, CSS_ATRULE);
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		/* Handle rulesets */
		if (*css == '*'			/* Wilcard element (*.class) */
			|| *css == '.'		/* Class definition (.class) */
			|| *css == ':'		/* Pseudo class (:hover) */
			|| *css == '#'		/* Id selector (#id) */
			|| *css == '['		/* Attribute selector (#id) */
			|| (IS_IDENT_START	/* Element name (h1) */
				& css_scan_table[*css])) {

			pstate->data.stylesheet.no_imports = 1;
			pstate = css_state_push(state, CSS_RULESET);
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		/* Eat start and end of sgml comments */
		if (*css == '<') {
			if (css_len < 4) return PSTATE_SUSPEND;

			if (!strncasecmp((css+1), "!--", 3)) {
				css += 4; css_len -= 4;
				continue;
			}
		}

		if (*css == '-') {
			if (css_len < 3) return PSTATE_SUSPEND;

			if (!strncasecmp((css+1), "->", 2)) {
				css += 3; css_len -= 3;
				continue;
			}
		}

		/* Error recovering: (not stack recovering) */
		css_state_push(state, CSS_SKIP);
		*src = css, *len = css_len;
		return PSTATE_CHANGE;
	}

	*src = css, *len = css_len;
	return PSTATE_SUSPEND;
}

typedef enum pstate_code (*parse_func)(struct parser_state *, unsigned char **, int *);

/* XXX: Keep in alphabetical order */
parse_func css_parsers[CSS_STATE_CODES] = {
	/* CSS_ATRULE */		css_parse_atrule,
	/* CSS_CHARSET */		css_parse_charset,
	/* CSS_COMMENT */		css_scan_comment,
	/* CSS_DECLARATION */		css_parse_declaration,
	/* CSS_DECLARATIONS */		css_parse_declarations,
	/* CSS_ESCAPE */		css_scan_escape,
	/* CSS_EXPRESSION */		css_parse_expression,
	/* CSS_FONTFACE */		css_parse_fontface,
	/* CSS_FUNCTION */		css_parse_function,
	/* CSS_HEXCOLOR */		css_scan_hexcolor,
	/* CSS_IDENT */			css_scan_ident,
	/* CSS_IMPORT */		css_parse_import,
	/* CSS_MEDIA */			css_parse_media,
	/* CSS_MEDIATYPES */		css_parse_mediatypes,
	/* CSS_NAME */			css_scan_name,
	/* CSS_PAGE */			css_parse_page,
	/* CSS_RGB */			css_parse_rgb,
	/* CSS_RULESET */		css_parse_ruleset,
	/* CSS_SELECTOR */		css_parse_selector,
	/* CSS_SIMPLE_SELECTOR */	css_parse_simple_selector,
	/* CSS_SELECTOR_ATTR */		css_parse_selector_attr,
	/* CSS_SKIP */			css_scan_skip,
	/* CSS_SKIP_MEDIATYPES */	css_parse_mediatypes,
	/* CSS_SKIP_UNTIL */		css_scan_skip_until,
	/* CSS_STRING */		css_scan_string,
	/* CSS_STYLESHEET */		css_parse_stylesheet,
	/* CSS_UNICODERANGE */		css_scan_unicoderange,
	/* CSS_URL */			css_scan_url,
};

/* The CSS backend hooks */

/* It is only possible to initialize the parser to start parsing of CSS
 * stylesheets. If only parsing of a subset of a stylesheet then calls should
 * go directly to the subset parser. See elusive/parser/html/ */
static void
init_css_parser(struct parser_state *state)
{
	struct css_parser_state *pstate;

	pstate = css_state_push(state, CSS_STYLESHEET);
	if (!pstate) return;

	state->real_root = init_stylesheet();

#ifdef CSS_DEBUG
	css_stack_size = 0;
#endif
}

static void
parse_css(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;

	while (*len) {
#ifdef CSS_DEBUG
		print_state(state, *src, *len);
#endif
		assert(pstate->state >= CSS_STATE_CODES);

		if (css_parsers[pstate->state](state, src, len) == PSTATE_SUSPEND) {
			return;
		}

		pstate = state->data; /* update to the top of the stack */
	}
}

static void
done_css_parser(struct parser_state *state)
{
	struct css_parser_state *pstate = state->data;

	while (pstate) {
		/* TODO Give state parsers signal (len = -1) to clean up */

		css_state_pop(state);
		pstate = state->data;
	}

	done_stylesheet(state->real_root);
}

struct parser_backend css_parser_backend = {
	init_css_parser,
	parse_css,
	done_css_parser,
};
