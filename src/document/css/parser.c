/* CSS main parser */
/* $Id: parser.c,v 1.63 2004/01/27 02:05:04 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/parser.h"
#include "document/css/property.h"
#include "document/css/scanner.h"
#include "document/css/stylesheet.h"
#include "document/css/value.h"
#include "document/html/parser.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


void
css_parse_properties(struct list_head *props, struct css_scanner *scanner)
{
	assert(props && scanner);

	while (css_scanner_has_tokens(scanner)) {
		struct css_property_info *property_info = NULL;
		struct css_property *prop;
		struct css_token *token = get_css_token(scanner);
		int i;

		if (!token || token->type == '}') break;

		/* Extract property name. */

		if (token->type != CSS_TOKEN_IDENT
		    || !check_next_css_token(scanner, ':')) {
			/* Some use style="{ properties }" so we have to be
			 * check what to skip to. */
			if (token->type == '{') {
				skip_css_tokens(scanner, '{');
			} else {
				skip_css_tokens(scanner, ';');
			}
			continue;
		}

		for (i = 0; css_property_info[i].name; i++) {
			struct css_property_info *info = &css_property_info[i];

			if (css_token_strlcasecmp(token, info->name, -1)) {
				property_info = info;
				break;
			}
		}

		/* Skip property name and separator and check for expression */
		if (!skip_css_tokens(scanner, ':')) {
			assert(!css_scanner_has_tokens(scanner));
			break;
		}

		if (!property_info) {
 			/* Unknown property, check the next one. */
 			goto ride_on;
 		}

		/* We might be on track of something, cook up the struct. */

		prop = mem_calloc(1, sizeof(struct css_property));
		if (!prop) {
			goto ride_on;
		}
		prop->type = property_info->type;
		prop->value_type = property_info->value_type;
		if (!css_parse_value(property_info, &prop->value, scanner)) {
			mem_free(prop);
			goto ride_on;
		}
		add_to_list(*props, prop);

		/* Maybe we have something else to go yet? */

ride_on:
		skip_css_tokens(scanner, ';');
	}
}


/* TODO: We should handle suppoert for skipping blocks better like "{ { } }"
 * will be handled correctly. --jonas */
#define skip_css_block(scanner) \
	if (skip_css_tokens(scanner, '{')) skip_css_tokens(scanner, '}');


/* Atrules grammer:
 *
 * media_types:
 *	  <empty>
 *	| <ident>
 *	| media_types ',' <ident>
 *
 * atrule:
 * 	  '@charset' <string> ';'
 *	| '@import' <string> media_types ';'
 *	| '@import' <uri> media_types ';'
 *	| '@media' media_types '{' ruleset* '}'
 *	| '@page' <ident>? [':' <ident>]? '{' properties '}'
 *	| '@font-face' '{' properties '}'
 */
static void
css_parse_atrule(struct css_stylesheet *css, struct css_scanner *scanner)
{
	struct css_token *token = get_css_token(scanner);

	/* Skip skip skip that code */
	switch (token->type) {
		case CSS_TOKEN_AT_IMPORT:
			token = get_next_css_token(scanner);
			if (!token) break;

			if (token->type == CSS_TOKEN_STRING
			    || token->type == CSS_TOKEN_URL) {
				assert(css->import);
				css->import(css, token->string, token->length);
			}
			skip_css_tokens(scanner, ';');
			break;

		case CSS_TOKEN_AT_CHARSET:
			skip_css_tokens(scanner, ';');
			break;

		case CSS_TOKEN_AT_FONT_FACE:
		case CSS_TOKEN_AT_MEDIA:
		case CSS_TOKEN_AT_PAGE:
			skip_css_block(scanner);
			break;

		case CSS_TOKEN_AT_KEYWORD:
			/* TODO: Unkown @-rule so either skip til ';' or next block. */
			while (css_scanner_has_tokens(scanner)) {
				token = get_next_css_token(scanner);

				if (token->type == ';') {
					skip_css_tokens(scanner, ';');
					break;

				} else if (token->type == '{') {
					skip_css_block(scanner);
					break;
				}
			}
		default:
			INTERNAL("@-rule parser called without atrule.");
	}
}


struct selector_pkg {
	LIST_HEAD(struct selector_pkg);
	struct css_selector *selector;
};

/* Selector grammar:
 *
 * selector:
 *	  (element_name id? class? pseudo_class?)+
 *	  | '#' ('.' class)? (':' pseudo_class)?
 *	  | '.' class (':' pseudo_class)?
 *	  | ':' pseudo_class
 *
 * TODO: selector can currently only be simple element names, and element
 * chains are not supported yet.
 */
static struct list_head *
css_parse_selector(struct css_stylesheet *css, struct css_scanner *scanner)
{
	struct css_token *token = get_css_token(scanner);
	static struct list_head selectors;
	struct selector_pkg *pkg;
	struct css_selector *selector;

	init_list(selectors);

	/* TODO: selector is (<element>)?([#:.]<ident>)?, not just <element>.
	 * And anyway we should have css_parse_selector(). --pasky */
	/* TODO: comma-separated list of simple selectors. */
	/* FIXME: element can be even '*' --pasky */

next_one:
	pkg = NULL;

	if (token->type != CSS_TOKEN_IDENT) {
		skip_css_tokens(scanner, '}');
		return NULL;
	}

	/* Check if we have already encountered the selector */
	/* FIXME: This is totally broken because we have to do this _after_
	 * scanning for id/class/pseudo. --pasky */
	selector = get_css_selector(css, token->string, token->length);
	if (!selector)
		goto out_of_memory;

	pkg = mem_alloc(sizeof(struct selector_pkg));
	if (!pkg)
		goto out_of_memory;
	pkg->selector = selector;

	/* Let's see if we will get anything else of this. */

	token = get_next_css_token(scanner);

	if (token->type == CSS_TOKEN_HASH
	    || token->type == CSS_TOKEN_HEX_COLOR) {
		/* id */
		selector->id = memacpy(token->string + 1, token->length - 1);
		token = get_next_css_token(scanner);
	}

	if (token->type == '.') {
		/* class */
		token = get_next_css_token(scanner);
		if (token->type != CSS_TOKEN_IDENT) {
			goto syntax_error;
		}
		selector->class = memacpy(token->string, token->length);
		token = get_next_css_token(scanner);
	}

	if (token->type == ':') {
		/* pseudo */
		token = get_next_css_token(scanner);
		if (token->type != CSS_TOKEN_IDENT) {
			goto syntax_error;
		}
		selector->pseudo = memacpy(token->string, token->length);
		token = get_next_css_token(scanner);
	}

	add_to_list(selectors, pkg);

	if (token->type == ',') {
		/* Multiple elements hooked up to this ruleset. */
		token = get_next_css_token(scanner);
		goto next_one;
	}

	if (token->type != '{') {
syntax_error:
                if (selector->id) mem_free(selector->id);
                if (selector->class) mem_free(selector->class);
                if (selector->pseudo) mem_free(selector->pseudo);
		if (pkg) mem_free(pkg);

out_of_memory:
		foreach (pkg, selectors) {
			selector = pkg->selector;
			if (selector->id) mem_free(selector->id);
			if (selector->class) mem_free(selector->class);
			if (selector->pseudo) mem_free(selector->pseudo);
		}
		free_list(selectors);

		skip_css_block(scanner);
		return NULL;
	}

	return &selectors;
}


/* Ruleset grammar:
 *
 * ruleset:
 *	  selector [ ',' selector ]* '{' properties '}'
 */
static void
css_parse_ruleset(struct css_stylesheet *css, struct css_scanner *scanner)
{
	struct selector_pkg *pkg, *fpkg;
	struct list_head *selectors;

	selectors = css_parse_selector(css, scanner);
	if (!selectors || list_empty(*selectors)) {
		return;
	}

	skip_css_tokens(scanner, '{');

	/* We don't handle the case where a property has already been added to
	 * a selector. That doesn't matter though, because the best one will be
	 * always the last one (FIXME: 'important!'), therefore the applier
	 * will take it last and it will have the "final" effect.
	 *
	 * So it's only a little waste and no real harm. The thing is, what do
	 * you do when you have 'background: #fff' and then 'background:
	 * x-repeat'? It would require yet another logic to handle merging of
	 * these etc and the induced overhead would in most cases mean more
	 * waste that having the property multiple times in a selector, I
	 * believe. --pasky */

	pkg = selectors->next;
	css_parse_properties(&pkg->selector->properties, scanner);

	skip_css_tokens(scanner, '}');

	/* Mirror the properties to all the selectors. */
	fpkg = pkg; pkg = pkg->next;
	while ((struct list_head *) pkg != selectors) {
		mirror_css_selector(fpkg->selector, pkg->selector);
		pkg = pkg->next;
	}
}


void
css_parse_stylesheet(struct css_stylesheet *css, unsigned char *string)
{
	struct css_scanner scanner;

	init_css_scanner(&scanner, string);

	while (css_scanner_has_tokens(&scanner)) {
		struct css_token *token = get_css_token(&scanner);

		assert(token);

		switch (token->type) {
		case CSS_TOKEN_AT_KEYWORD:
		case CSS_TOKEN_AT_CHARSET:
		case CSS_TOKEN_AT_FONT_FACE:
		case CSS_TOKEN_AT_IMPORT:
		case CSS_TOKEN_AT_MEDIA:
		case CSS_TOKEN_AT_PAGE:
			css_parse_atrule(css, &scanner);
			break;

		default:
			/* And WHAT ELSE could it be?! */
			css_parse_ruleset(css, &scanner);
		}
	}
}
