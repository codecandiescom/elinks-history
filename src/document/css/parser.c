/* CSS main parser */
/* $Id: parser.c,v 1.21 2004/01/18 18:21:06 jonas Exp $ */

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
