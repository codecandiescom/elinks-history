/* CSS stylesheet handling */
/* $Id: stylesheet.c,v 1.13 2004/01/27 00:23:46 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/property.h"
#include "document/css/stylesheet.h"
#include "util/error.h"
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

struct css_selector *
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

static struct css_selector *
copy_css_selector(struct css_stylesheet *css, struct css_selector *orig)
{
	struct css_selector *copy;

	assert(css && orig);

	copy = init_css_selector(css, orig->element, strlen(orig->element));
	if (!copy)
		return NULL;

	if (orig->id) copy->id = stracpy(orig->id);
	if (orig->class) copy->class = stracpy(orig->class);
	if (orig->pseudo) copy->pseudo = stracpy(orig->pseudo);

	return copy;
}

static struct css_selector *
clone_css_selector(struct css_stylesheet *css, struct css_selector *orig)
{
	struct css_selector *copy;
	struct css_property *prop;

	assert(css && orig);

	copy = copy_css_selector(css, orig);
	if (!copy)
		return NULL;

	foreach (prop, orig->properties) {
		struct css_property *newprop;

		newprop = mem_calloc(1, sizeof(struct css_property));
		if (!newprop)
			continue;
		*newprop = *prop;
		add_to_list(copy->properties, newprop);
	}

	return copy;
}

void
merge_css_selectors(struct css_selector *sel1, struct css_selector *sel2)
{
	struct css_property *prop;

	foreach (prop, sel2->properties) {
		struct css_property *origprop;

		foreach (origprop, sel1->properties)
			if (origprop->type == prop->type)
				goto found;

		/* Not there yet, let's add it. */
		origprop = mem_calloc(1, sizeof(struct css_property));
		if (!origprop)
			continue;
		*origprop = *prop;
		add_to_list(sel1->properties, origprop);

found:
		continue;
	}
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
