/* CSS stylesheet handling */
/* $Id: stylesheet.c,v 1.1 2004/01/24 02:05:46 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/property.h"
#include "document/css/stylesheet.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


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
