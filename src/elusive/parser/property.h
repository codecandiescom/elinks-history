/* $Id: property.h,v 1.2 2003/01/18 00:36:14 pasky Exp $ */

#ifndef EL__USIVE_PARSER_PROPERTY_H
#define EL__USIVE_PARSER_PROPERTY_H

#include "util/lists.h"

enum property_type {
	PROPERTY_STRING,
	PROPERTY_INT,
	PROPERTY_COLOR,
};

struct property {
	struct property *next;
	struct property *prev;

	unsigned char *name; int namelen;
	unsigned char *value; int valuelen;
};

/* Get property of a given name from the supplied list. Returns NULL on
 * failure (ie. the property doesn't exist in the list). */
struct property *
get_property(struct list_head *properties, unsigned char *name);

/* Add a new property to the supplied list. */
struct property *
add_property(struct list_head *properties, unsigned char *name, int namelen,
             unsigned char *value, int valuelen);

#endif
