/* Properties utility tools */
/* $Id: property.c,v 1.2 2003/01/18 00:36:14 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/property.h"
#include "util/lists.h"
#include "util/string.h"


struct property *
get_property(struct list_head *properties, unsigned char *name)
{
	struct property *property;

	foreach (property, *properties) {
		if (strncasecmp(name, property->name, property->namelen))
			continue;

		return property;
	}

	return NULL;
}

struct property *
add_property(struct list_head *properties, unsigned char *name, int namelen,
             unsigned char *value, int valuelen)
{
	struct property *property = mem_calloc(1, sizeof(struct property));

	if (!property) return NULL;

	property->name = name, property->namelen = namelen;
	property->value = value, property->valuelen = valuelen;
	add_to_list(*properties, property);

	return property;
}
