/* CSS main parser */
/* $Id: parser.c,v 1.22 2004/01/19 17:03:49 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/parser.h"
#include "document/css/property.h"
#include "document/css/scanner.h"
#include "document/css/value.h"
#include "document/html/parser.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


void
css_parse_properties(struct list_head *props, unsigned char *string)
{
	struct css_scanner scanner;

	assert(props && string);

	init_css_scanner(&scanner, string);

	while (css_scanner_has_tokens(&scanner)) {
		struct css_property_info *property_info = NULL;
		struct css_property *prop;
		struct css_token *token = get_css_token(&scanner);
		int i;

		if (!token) break;

		/* Extract property name. */

		if (token->type != CSS_TOKEN_IDENTIFIER
		    || !check_next_css_token(&scanner, ':')) {
			skip_css_tokens(&scanner, ';');
			continue;
		}

		for (i = 0; css_property_info[i].name; i++) {
			struct css_property_info *info = &css_property_info[i];

			if (css_token_strlcasecmp(token, info->name, -1)) {
				property_info = info;
				break;
			}
		}

		if (!skip_css_tokens(&scanner, ':')) {
			INTERNAL("I thought we already knew ':' was the next token");
			goto ride_on;
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
		if (!css_parse_value(property_info, &prop->value, &scanner)) {
			mem_free(prop);
			goto ride_on;
		}
		add_to_list(*props, prop);

		/* Maybe we have something else to go yet? */

ride_on:
		skip_css_tokens(&scanner, ';');
	}
}

/* TODO: We should handle suppoert for skipping blocks better like "{ { } }"
 * will be handled correctly. --jonas */
#define skip_css_block(scanner) \
	if (skip_css_tokens(scanner, '{')) skip_css_tokens(scanner, '}');

static void
css_parse_atrule(struct css_stylesheet *css, struct css_scanner *scanner)
{
	struct css_token *token = get_css_token(scanner);

	/* Skip skip skip that code */

	if (css_token_contains(token, "@charset")
	    || css_token_contains(token, "@import")) {
		skip_css_tokens(scanner, ';');

	} else if (css_token_contains(token, "@media")
		   || css_token_contains(token, "@font")
		   || css_token_contains(token, "@page")
		   || css_token_contains(token, "@font-face")) {
		skip_css_block(scanner);

	} else {
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
	}
}

static void
css_parse_selector(struct css_stylesheet *css, struct css_scanner *scanner)
{
	struct css_token *token = get_css_token(scanner);
	struct css_selector *selector;

	if (!check_next_css_token(scanner, '{')) {
		/* TODO: comma separated list of simple selectors. */
		skip_css_tokens(scanner, '}');
		return;
	}

	selector = mem_calloc(1, sizeof(struct css_selector));
	if (!selector) return;

	init_list(selector->properties);

	selector->element = memacpy(token->string, token->length);
	if (!selector->element) {
		mem_free(selector);
		return;
	}

	skip_css_tokens(scanner, '{');

	token = get_css_token(scanner);
	assert(token);

	css_parse_properties(&selector->properties, token->string);

	skip_css_tokens(scanner, '}');

	add_to_list(css->selectors, selector);
}

void
css_parse_stylesheet(struct css_stylesheet *css, unsigned char *string)
{
	struct css_scanner scanner;

	init_css_scanner(&scanner, string);

	while (css_scanner_has_tokens(&scanner)) {
		struct css_token *token = get_css_token(&scanner);

		assert(token);

		if (token->type == CSS_TOKEN_IDENTIFIER) {
			/* Handle more selectors like '*' ':<ident>' to start */
			css_parse_selector(css, &scanner);

		} else if (token->type == CSS_TOKEN_ATRULE) {
			css_parse_atrule(css, &scanner);

		} else if (token->type == '<') {
			/* The scanner will generate SGML comment tokens so
			 * this must be some ending <style> tag so bail out. */
			return;

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
