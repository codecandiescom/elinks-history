/* CSS main parser */
/* $Id: parser.c,v 1.39 2004/01/24 02:05:46 jonas Exp $ */

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
			skip_css_tokens(scanner, ';');
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
		case CSS_TOKEN_CHARSET:
		case CSS_TOKEN_IMPORT:
			skip_css_tokens(scanner, ';');
			break;

		case CSS_TOKEN_FONT_FACE:
		case CSS_TOKEN_MEDIA:
		case CSS_TOKEN_PAGE:
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

/* Ruleset grammar:
 *
 * ruleset:
 *	  selector [ ',' selector ]* '{' properties '}'
 *
 * TODO: selector can currently only be simple element names and we don't even
 * handle comma separated list of selectors yet.
 */
static void
css_parse_ruleset(struct css_stylesheet *css, struct css_scanner *scanner)
{
	struct css_token *token = get_css_token(scanner);
	struct css_selector *selector;

	if (!check_next_css_token(scanner, '{')) {
		/* TODO: comma separated list of simple selectors. */
		skip_css_tokens(scanner, '}');
		return;
	}

	/* Check if we have already encountered the selector */
	selector = get_css_selector(css, token->string, token->length);
	if (!selector) {
		selector = mem_calloc(1, sizeof(struct css_selector));
		if (!selector) return;

		init_list(selector->properties);

		selector->element = memacpy(token->string, token->length);
		if (!selector->element) {
			mem_free(selector);
			return;
		}

		add_to_list(css->selectors, selector);
	}

	skip_css_tokens(scanner, '{');

	/* TODO: We don't handle the case where a property has already been
	 * added to a selector. Maybe we should pass an empty list to
	 * css_parse_properties() and then do a merge of old and new properties
	 * favoring the new ones. */
	css_parse_properties(&selector->properties, scanner);

	skip_css_tokens(scanner, '}');
}

struct css_selector *
get_css_selector(struct css_stylesheet *css, unsigned char *name, int namelen)
{
	struct css_selector *selector;

	foreach (selector, css->selectors) {
		if (!strlcasecmp(name, namelen, selector->element, -1))
			return selector;
	}

	return NULL;
}

void
css_parse_stylesheet(struct css_stylesheet *css, unsigned char *string)
{
	struct css_scanner scanner;

	init_css_scanner(&scanner, string);

	while (css_scanner_has_tokens(&scanner)) {
		struct css_token *token = get_css_token(&scanner);

		assert(token);

		if (token->type == CSS_TOKEN_IDENT) {
			/* TODO: Handle more selectors like '*' ':<ident>' */
			css_parse_ruleset(css, &scanner);

		} else if (token->type == CSS_TOKEN_AT_KEYWORD) {
			css_parse_atrule(css, &scanner);

		} else {
			/* TODO: Skip to ';' or block if '{' */
			skip_css_tokens(&scanner, token->type);
		}
	}
}

void
done_css_stylesheet(struct css_stylesheet *css)
{
	while (!list_empty(css->selectors)) {
		struct css_selector *selector = css->selectors.next;

		del_from_list(selector);

		while (!list_empty(selector->properties)) {
			struct css_property *prop = selector->properties.next;

			del_from_list(prop);
			mem_free(prop);
		}

		mem_free(selector->element);
		mem_free(selector);
	}
}
