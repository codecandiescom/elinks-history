/* CSS stylesheet handling */
/* $Id: stylesheet.c,v 1.3 2004/01/24 03:35:15 jonas Exp $ */

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

struct css_selector *
init_css_selector(struct css_stylesheet *css, unsigned char *name, int namelen)
{
	struct css_selector *selector;

	selector = mem_calloc(1, sizeof(struct css_selector));
	if (!selector) return NULL;

	init_list(selector->properties);

	selector->element = memacpy(name, namelen);
	if (!selector->element) {
		mem_free(selector);
		return NULL;
	}

	add_to_list(css->selectors, selector);

	return selector;
}

void
done_css_stylesheet(struct css_stylesheet *css)
{
	while (!list_empty(css->selectors)) {
		struct css_selector *selector = css->selectors.next;

		del_from_list(selector);
		free_list(selector->properties);
		mem_free(selector->element);
		mem_free(selector);
	}
}
