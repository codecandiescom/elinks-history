/* CSS ruleset parsing */
/* $Id: ruleset.c,v 1.4 2003/07/06 23:17:34 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/property.h"
#include "elusive/parser/css/parser.h"
#include "elusive/parser/css/ruleset.h"
#include "elusive/parser/css/scanner.h"
#include "elusive/parser/css/state.h"
#include "elusive/parser/css/tree.h"
#include "elusive/parser/parser.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"

#ifdef CSS_DEBUG
#include <stdio.h>
#include "elusive/parser/css/test.h"
#endif

int selector_counter = 0;
/* Ruleset grammar:
 *
 * ruleset:
 *	  selector [ ',' selector ]* '{' declarations '}'
 */
enum pstate_code
css_parse_ruleset(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	/* Add selector node to list of selectors. */
	/* Note we use the environment next/prev pointers
	 * TODO write macros similar to the ones in util/lists.h */
	if (state->current) {
		struct css_node *node = state->current;

		if (!pstate->data.ruleset.selectors) {
			node->next_env = node->prev_env = node;
			pstate->data.ruleset.selectors = node;
		} else {
			struct css_node *node_list;

			node_list = pstate->data.ruleset.selectors;
			node->next_env = node_list->next_env;
			node->prev_env = node_list;
			node_list->next_env = node;
			node->next_env->prev_env = node;
		}

		printf("Selectors [%d]\n", ++selector_counter);
		state->current = state->root = NULL;
	}

	/* Get selectors */
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
			css_state_push(state, CSS_SELECTOR);
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		/* A sign that there are more selectors */
		if (*css == ',') {
			/* We have to get rid of whitespace before we
			 * can handle the following selector */
			css++, css_len--;
			continue;
		}

		/* Entering the block. Setup declaration parsing */
		if (*css == '{') {
			struct css_node *nodes = pstate->data.ruleset.selectors;

			/* Prepare for returning from declaration parsing */
			pstate = css_state_repush(state, CSS_SKIP);
			pstate->data.skip.nest_level = 1;

			pstate = css_state_push(state, CSS_DECLARATIONS);
			pstate->data.declarations.nodes = nodes;

			css++, css_len--;
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		/* Error recovery */
		*src = css, *len = css_len;
		css_state_repush(state, CSS_SKIP);
		return PSTATE_CHANGE;
	}

	*src = css, *len = css_len;
	return PSTATE_SUSPEND;
}

/* Selector grammar:
 *
 * selector:
 *	  simple_selector
 *	| simple_selector combinator selector
 *
 * combinator:
 *	  <empty>
 *	| '>'
 *	| '+'
 */
/* We do not check if two '>'s follow each other etc. */
enum pstate_code
css_parse_selector(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;
	enum css_elem_match_type match_type = pstate->data.selector.match_type;

	/* Check if anything was updated in the work node. If nothing was
	 * updated we should fall through to the 'No more selectors' section
	 * below */
	if (match_type) {
		struct css_node *node = state->current;

		if (!node) {
			/* Error that no selector was found. Signal this. */
			/* TODO possibly descend and detach empty nodes */
			/* TODO Skip til ',' or '{' */
			css_state_pop(state);
			return PSTATE_CHANGE;
		}

		/* Contribute the match type and reset so it's not reused */
		node->match_type = match_type;
		match_type = 0;
	}

	while (css_len) { 
		CSS_SKIP_WHITESPACE(css, css_len);
		CSS_SKIP_COMMENT(state, src, len, css, css_len);

		/* Handle element name */
		if (css_scan_table[*css] & IS_IDENT_START) {
			if (!match_type) match_type = MATCH_DESCENDANT;
			pstate->data.selector.match_type = match_type;

			css_state_push(state, CSS_SIMPLE_SELECTOR);
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		/* Handle the universal selector starts */
		if (*css == '*'
			|| *css == '['
			|| *css == '.'
			|| *css == ':'
			|| *css == '#') {
			if (!match_type) match_type = MATCH_DESCENDANT;
			match_type |= MATCH_UNIVERSAL;
			pstate->data.selector.match_type = match_type;

			css_state_push(state, CSS_SIMPLE_SELECTOR);
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		if (*css == '+') {
			match_type = MATCH_ADJACENT_SIBLING;
			css++, css_len--;
			continue;
		}

		if (*css == '>') {
			match_type = MATCH_CHILD;
			css++, css_len--;
			continue;
		}

		/* No more selectors */
		css_state_pop(state);
		*src = css, *len = css_len;
		return PSTATE_CHANGE;
	}

	*src = css, *len = css_len;
	return PSTATE_SUSPEND;
}

/* Simple selector grammar:
 *
 * simple_selector:
 *	  simple_selector_start simple_selector_ending
 *
 * simple_selector_start:
 *	  element_name
 *	| simple_selector_attr
 *
 * element_name:
 *	  <ident>
 *	| '*'
 */
/* This parses the simple selector start */
enum pstate_code
css_parse_simple_selector(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	/* Handle failure from scanner */
	if (pstate->data.simple_selector.name_len < 0) {
		/* Signal that no simple selector was found */
		state->root = state->current;
		state->current = NULL;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	/* Handle the token containing the element name */
	if (pstate->data.simple_selector.name_len > 0) {
		struct css_node *node = spawn_css_node(state,
					pstate->data.simple_selector.name,
					pstate->data.simple_selector.name_len);

		if (!node) {
			css_state_pop(state);
			return PSTATE_CHANGE;
		}

		css_state_repush(state, CSS_SELECTOR_ATTR);
		*src = css, *len = css_len;
		return PSTATE_CHANGE;
	}

	while (css_len) { 
		/* Get the element name */
		if (css_scan_table[*css] & IS_IDENT_START) {
			unsigned char **name = &(pstate->data.simple_selector.name);
			int *name_len = &(pstate->data.simple_selector.name_len);

			pstate = css_state_push(state, CSS_IDENT);
			pstate->data.token.str = name;
			pstate->data.token.len = name_len;
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		/* The element is the universal selector */
		if (*css == '*'
			|| *css == '['
			|| *css == '.'
			|| *css == ':'
			|| *css == '#') {
			struct css_node *node = state->current;

			if (!node) {
				/* There's no previous nodes to build on so
				 * this is the toplevel universal selector*/
				struct stylesheet *stylesheet = state->real_root;

				/* Get universal top node */
				state->root = state->current = stylesheet->universal;
			} else {
				node = spawn_css_node(state, "*", 1);

				if (!node) {
					/* Signal no simple selector was found */
					state->current = NULL;
					css_state_pop(state);
					return PSTATE_CHANGE;
				}
			}

			if (*css == '*') {
				css++; css_len--;
			}

			css_state_repush(state, CSS_SELECTOR_ATTR);
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		/* No more selectors */
		css_state_pop(state);
		*src = css, *len = css_len;
		return PSTATE_CHANGE;
	}

	*src = css, *len = css_len;
	return PSTATE_SUSPEND;
}

/* Simple selector part grammar:
 *
 * simple_selector_ending:
 *	  <empty>
 *	| simple_selector_attr simple_selector_ending
 *
 * simple_selector_attr:
 *	  '#' <name>				-- id attributes
 *	| '.' <ident>				-- class attributes
 *	| '[' <ident> attribute_matching ']'	-- matching markup attributes
 *	| ':' pseudo				-- e.g. :hover, :first-line etc.
 *
 * attribute_matching:
 *	  <empty>
 *	| '=' [ <ident> | <string> ]
 *	| '~=' [ <ident> | <string> ]
 *	| '|=' [ <ident> | <string> ]
 *
 * pseudo:
 *	  <ident>
 *	| <function> expression ')'
 */
enum pstate_code
css_parse_selector_attr(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;
	enum attribute_match_type match_type;
	struct attribute_matching *attr;
	
	match_type = pstate->data.simple_selector.match_type;
	attr = pstate->data.simple_selector.attr;

	/* Handle failure from scanner */
	if (pstate->data.simple_selector.name_len < 0) {
		if (attr) {
			del_from_list(attr);
			mem_free(attr);
		}

		/* Signal that no simple selector was found */
		state->root = state->current;
		state->current = NULL;
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	/* Handle the token containing the element name */
	if (pstate->data.simple_selector.name_len > 0) {
		if (attr) {
			/* The token is the attribute value */
			attr->value = pstate->data.simple_selector.name;
			attr->value_len = pstate->data.simple_selector.name_len;

			/* Skip whitespace/comments after attribute match
			 * values until ']' */
			if (match_type & MATCH_ATTRIBUTE) {
				while (css_len) {
					CSS_SKIP_WHITESPACE(css, css_len);
					CSS_SKIP_COMMENT(state, src, len, css, css_len);

					if (*css == ']') {
						css++; css_len--;
						break;
					}

					/* Error handling */
					state->root = state->current;
					state->current = NULL;
					*src = css, *len = css_len;
					css_state_pop(state);
					return PSTATE_CHANGE;
				}
			}

			print_token("attribute value",
				pstate->data.simple_selector.name,
				pstate->data.simple_selector.name_len);

			attr = NULL;
			match_type = 0;
		} else {
			attr = spawn_css_attr_match(state, match_type);

			if (!attr) {
				css_state_pop(state);
				return PSTATE_CHANGE;
			}

			attr->name = pstate->data.simple_selector.name;
			attr->name_len = pstate->data.simple_selector.name_len;

			print_token("attribute",  pstate->data.simple_selector.name,
				pstate->data.simple_selector.name_len);

			/* TODO parse the ~=, |= or =  */

		}
	}

	while (css_len) {
		/* Comments and whitespace are allowed only in attribute
		 * matches ('[' ... ']') */
		if (match_type & MATCH_ATTRIBUTE) {
			CSS_SKIP_WHITESPACE(css, css_len);
			CSS_SKIP_COMMENT(state, src, len, css, css_len);
		}

		/* Get the element name */
		if (match_type && css_scan_table[*css] & IS_IDENT_START) {
			unsigned char **name = &(pstate->data.simple_selector.name);
			int *name_len = &(pstate->data.simple_selector.name_len);

			pstate->data.simple_selector.match_type = match_type;
			pstate->data.simple_selector.attr = attr;

			pstate = css_state_push(state, CSS_IDENT);
			pstate->data.token.str = name;
			pstate->data.token.len = name_len;

			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		/* The element is the universal selector */
		if (*css == '.' && !attr) {
			attr = spawn_css_attr_match(state, MATCH_CLASS);
			attr->name = "class";
			attr->name_len = 5;

			css++; css_len--;
			continue;
		}

		if (*css == '#' && !attr) {
			attr = spawn_css_attr_match(state, MATCH_ID);
			attr->name = "id";
			attr->name_len = 2;

			css++; css_len--;
			continue;
		}

		if (*css == ':') {
			match_type = MATCH_PSEUDO_CLASS;
			css++; css_len--;
			continue;
		}

		if (*css == '[') {
			match_type = MATCH_ATTRIBUTE;
			css++; css_len--;
			continue;
		}

		/* No more selector attributes */
		css_state_pop(state);
		*src = css, *len = css_len;
		return PSTATE_CHANGE;
	}

	*src = css, *len = css_len;
	return PSTATE_SUSPEND;
}

/* Declarations grammar:
 *
 * declarations:
 *	  declaration
 *	| declaration ';' declaration 
 *
 */
enum pstate_code
css_parse_declarations(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	assert(pstate->data.declarations.nodes);
	/* TODO: if_assert_failed ! But avoid infinite loops ;-). --pasky */

	while (css_len) {
		struct css_node *node;
		struct css_node *selectors;

		CSS_SKIP_WHITESPACE(css, css_len);
		CSS_SKIP_COMMENT(state, src, len, css, css_len);

		/* Check if there's a declaration start */
		if (css_scan_table[*css] & IS_IDENT_START) {
			struct list_head **properties;

			properties = &(pstate->data.declarations.properties);
			pstate = css_state_push(state, CSS_DECLARATION);
			pstate->data.declaration.properties = properties;
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		if (*css == ';') {
			/* A sign that there are more declarations */
			/* We have to get rid of whitespace before we
			 * can handle the following declaration */
			css++, css_len--;
			continue;
		}

		/* No more declarations. Add properties to selector nodes. */
		selectors = pstate->data.declarations.nodes;
		node = selectors->next_env;

		do {
			struct css_node *wipe;

			if (selector_counter > 0) printf("Selector %d ", --selector_counter);
			print_token("node", node->str, node->strlen);
			wipe = node;
			node = node->next_env;
			wipe->next_env = wipe->prev_env = NULL;
		} while (node != selectors);

		*src = css, *len = css_len;
		pstate = css_state_pop(state);
		return PSTATE_CHANGE;
	}

	*src = css, *len = css_len;
	return PSTATE_SUSPEND;
}

/* Declaration grammar:
 *
 * <ident> is a property
 *
 * declarations:
 *	  declaration
 *	| declarations ';' declaration
 *
 * declaration:
 *	  <empty>
 *	| <ident> ':' expression [ '!' 'important' ]?
 */
/*
 * Declarations will be translated to attributes in the syntree_node therefore
 * we have to 'unfold' the compact way of writing some of the properties.
 * Example:
 * 
 * Some property declarations can be grouped together (padding, border etc.).
 * This is handled by 'unfolding' the compact form into an 'atomic' style
 * settings. Example:
 *
 * 	p { margin: 0em 2em; }
 *
 * will be broken up into:
 *
 * 	p { margin-top: 0em;  margin-bottom: 0em;
 * 	    margin-left: 2em; margin-right: 2em; }
 *
 * This way redeclarations will be easier.
 */
enum pstate_code
css_parse_declaration(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	while (css_len) { 
		CSS_SKIP_WHITESPACE(css, css_len);
		CSS_SKIP_COMMENT(state, src, len, css, css_len);

		/* Get the property */
		if (css_scan_table[*css] & IS_IDENT_START) {
			struct css_parser_state *nextstate;

			/* Let ident parser work */
			nextstate = css_state_push(state, CSS_IDENT);
			nextstate->data.token.str = &(pstate->data.mediatypes.name);
			nextstate->data.token.len = &(pstate->data.mediatypes.name_len);
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		/* Set state to handle expression */
		if (*css == ':') {
			css++; css_len--;
			*src = css, *len = css_len;
			/* TODO The expression should be passed the property id
			 * to be able to verify it */
			pstate = css_state_push(state, CSS_EXPRESSION);
			return PSTATE_CHANGE;
			continue;
		}
		/* TODO Maybe skip till next ';' or '}' */

		/* We're done */
		css_state_pop(state);
		*src = css, *len = css_len;
		return PSTATE_CHANGE;
	}

	*src = css, *len = css_len;
	return PSTATE_SUSPEND;
}

/* Expression grammar:
 *
 * expression:
 *	  term
 *	| expression operator term
 *
 * operator:
 *	  <empty>/
 *	| '-'
 *	| '+'
 *
 * term:
 *	  unary_operator <number>'%'
 *	| unary_operator <number>['px'|'cm'|'mm'|'in'|'pt'|'pc']
 *	| unary_operator <number>'em'
 *	| unary_operator <number>'ex'
 *	| unary_operator <number>['deg'|'rad'|'grad']
 *	| unary_operator <number>['ms'|'s']
 *	| unary_operator <number>['Hz'|'kHz']
 *	| unary_operator <number>
 *	| unary_operator <function>
 *	| unary_operator <identifier>
 *	| unary_operator <unknown>
 *	| <string>
 *	| <uri>
 *	| <rgbcolor>
 *	| <unicoderange>
 *	| <hexcolor>
 *
 * unary_operator:
 *	  <empty>/
 *	| '-'
 *	| '+'
 */
enum pstate_code
css_parse_expression(struct parser_state *state, unsigned char **src, int *len)
{
	struct css_parser_state *pstate = state->data;
	unsigned char *css = *src;
	int css_len = *len;

	while (css_len) { 
		CSS_SKIP_WHITESPACE(css, css_len);
		CSS_SKIP_COMMENT(state, src, len, css, css_len);

		/* Get next term */
		if (css_scan_table[*css] & IS_IDENT_START) {
			struct css_parser_state *nextstate;

			/* Let ident parser work */
			nextstate = css_state_push(state, CSS_IDENT);
			nextstate->data.token.str = &(pstate->data.mediatypes.name);
			nextstate->data.token.len = &(pstate->data.mediatypes.name_len);
			*src = css, *len = css_len;
			return PSTATE_CHANGE;
		}

		/* We're done */
		css_state_pop(state);
		return PSTATE_CHANGE;
	}

	*src = css, *len = css_len;
	return PSTATE_SUSPEND;
}

/* Function grammar:
 * 
 * function:
 *	  <ident> '(' expr ')'
 */
enum pstate_code
css_parse_function(struct parser_state *state, unsigned char **src, int *len)
{
	css_state_pop(state);
	return PSTATE_CHANGE;
}

/* The arguments to the rgb function has the following syntax:
 * 
 *	{w}{num}%?{w}","{w}{num}%?{w}","{w}{num}%?{w}
 *
 * First of all everything from 'rgb(' to ')' should be accepted
 * and afterwards verified. Maybe introduce a skipping function
 * up to next ')' */
enum pstate_code
css_parse_rgb(struct parser_state *state, unsigned char **src, int *len)
{
	css_state_pop(state);
	return PSTATE_CHANGE;
}
