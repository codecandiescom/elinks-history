/* CSS main parser */
/* $Id: parser.c,v 1.17 2004/01/18 14:25:13 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/parser.h"
#include "document/css/property.h"
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
	assert(props && string);

	while (*string) {
		struct css_property_info *property_info = NULL;
		struct css_property *prop;
		int pos, i;

		/* Align myself. */

		skip_whitespace(string);

		/* Extract property name. */

		pos = strcspn(string, ":;");
		if (string[pos] != ':') {
			string += pos + (string[pos] == ';');
			continue;
		}

		for (i = 0; css_property_info[i].name; i++) {
			struct css_property_info *info = &css_property_info[i];

			if (!strlcasecmp(string, pos, info->name, -1)) {
				property_info = info;
				break;
			}
		}

		string += pos + 1;

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
		if (!css_parse_value(prop->value_type, &prop->value, &string)) {
			mem_free(prop);
			goto ride_on;
		}
		add_to_list(*props, prop);

		/* Maybe we have something else to go yet? */

ride_on:
		pos = strcspn(string, ";");
		string += pos + (string[pos] == ';');
	}
}
