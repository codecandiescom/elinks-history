/* CSS stylesheet handling */
/* $Id: stylesheet.c,v 1.24 2004/04/16 16:32:49 zas Exp $ */

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


/* You can find some mysterious functions commented out here. I planned to use
 * them for various smart things (well they all report to
 * merge_css_stylesheets()), but it turns out it makes no sense to merge
 * stylesheets now (and maybe it won't in the future neither...). But maybe you
 * will find them useful at some time, so... Dunno. --pasky */


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
		if (namelen < 0)
			namelen = strlen(name);
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

void
add_selector_properties(struct css_selector *selector, struct list_head *properties)
{
	struct css_property *prop;

	foreach (prop, *properties) {
		struct css_property *newprop;

		newprop = mem_calloc(1, sizeof(struct css_property));
		if (!newprop)
			continue;
		*newprop = *prop;
		add_to_list(selector->properties, newprop);
	}
}

static struct css_selector *
clone_css_selector(struct css_stylesheet *css, struct css_selector *orig)
{
	struct css_selector *copy;

	assert(css && orig);

	copy = copy_css_selector(css, orig);
	if (!copy)
		return NULL;
	add_selector_properties(copy, &orig->properties);
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
	if (selector->next) del_from_list(selector);
	free_list(selector->properties);
	mem_free_if(selector->element);
	mem_free_if(selector->id);
	mem_free_if(selector->class);
	mem_free_if(selector->pseudo);
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
	init_list(css->selectors);
	return css;
}

void
mirror_css_stylesheet(struct css_stylesheet *css1, struct css_stylesheet *css2)
{
	struct css_selector *selector;

	foreach (selector, css1->selectors) {
		clone_css_selector(css2, selector);
	}
}

#if 0
struct css_stylesheet *
clone_css_stylesheet(struct css_stylesheet *orig)
{
	struct css_stylesheet *copy;
	struct css_selector *selector;

	copy = init_css_stylesheet(orig->import);
	if (!copy)
		return NULL;
	mirror_css_stylesheet(orig, copy);
	return copy;
}

void
merge_css_stylesheets(struct css_stylesheet *css1,
		      struct css_stylesheet *css2)
{
	struct css_selector *selector;

	assert(css1 && css2);

	/* This is 100% evil. And gonna be a huge bottleneck. Essentially
	 * O(N^2) where we could be much smarter (ie. sort it once and then
	 * always be O(N)). */

	foreach (selector, css2->selectors) {
		struct css_selector *origsel;

		origsel = find_css_selector(css1, selector->name,
					    strlen(selector->name));
		if (!origsel) {
			clone_css_selector(css1, selector);
		} else {
			merge_css_selectors(origsel, selector);
		}
	}
}
#endif

void
done_css_stylesheet(struct css_stylesheet *css)
{
	while (!list_empty(css->selectors)) {
		done_css_selector(css->selectors.next);
	}
}
