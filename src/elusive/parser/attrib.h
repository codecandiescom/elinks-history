/* $Id: attrib.h,v 1.4 2002/12/30 23:55:18 pasky Exp $ */

#ifndef EL__USIVE_PARSER_ATTRIB_H
#define EL__USIVE_PARSER_ATTRIB_H

#include "util/lists.h"

enum attribute_type {
	ATTR_STRING,
	ATTR_INT,
	ATTR_COLOR,
};

struct attribute {
	struct attribute *next;
	struct attribute *prev;

	unsigned char *name; int namelen;
	unsigned char *value; int valuelen;
};

/* Get attribute of a given name from the supplied list. Returns NULL on
 * failure (ie. the attribute doesn't exist in the list). */
struct attribute *
get_attrib(struct list_head attrs, unsigned char *name);

/* Add a new attribute to the supplied list. */
struct attribute *
add_attrib(struct list_head attrs, unsigned char *name, int namelen,
	   unsigned char *value, int valuelen);

#endif
