/* CSS stylesheet handling */
/* $Id: stylesheet.c,v 1.11 2004/01/27 00:13:03 pasky Exp $ */

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
find_css_selector(struct css_stylesheet *css, unsigned char *name, int namelen)
{
	struct css_selector *selector;

	assert(css && name);

	foreach (selector, css->selectors) {
		if (!strlcasecmp(name, namelen, selector->element, -1))
			return selector;
	}

	return NULL;
}

static struct css_selector *
init_css_selector(struct css_stylesheet *css, unsigned char *name, int namelen)
{
	struct css_selector *selector;

	selector = mem_calloc(1, sizeof(struct css_selector));
	if (!selector) return NULL;

	init_list(selector->properties);

	if (name) {
		selector->element = memacpy(name, namelen);
		if (!selector->element) {
			mem_free(selector);
			return NULL;
		}
	}

	if (css) {
		add_to_list(css->selectors, selector);
	}

	return selector;
}

struct css_selector *
get_css_selector(struct css_stylesheet *css, unsigned char *name, int namelen)
{
	struct css_selector *selector = NULL;

	if (css && name && namelen) {
		selector = find_css_selector(css, name, namelen);
		if (selector)
			return selector;
	}

	selector = init_css_selector(css, name, namelen);
	if (selector)
		return selector;

	return NULL;
}

void
done_css_selector(struct css_selector *selector)
{
	struct css_selector *selector = css->selectors.next;

	if (selector->next) del_from_list(selector);
	free_list(selector->properties);
	if (selector->element) mem_free(selector->element);
	if (selector->id) mem_free(selector->id);
	if (selector->class) mem_free(selector->class);
	if (selector->pseudo) mem_free(selector->pseudo);
	mem_free(selector);
}


struct css_stylesheet *
init_css_stylesheet(css_stylesheet_importer importer)
{
	struct css_stylesheet *css;

	css = mem_calloc(1, sizeof(struct css_stylesheet));
	if (!css)
		return NULL;
	css->import = importer;
	return css;
}

void
done_css_stylesheet(struct css_stylesheet *css)
{
	while (!list_empty(css->selectors)) {
		done_css_selector(css->selectors.next);
	}
}
