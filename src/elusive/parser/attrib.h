/* $Id: attrib.h,v 1.3 2002/12/26 02:31:48 pasky Exp $ */

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
 * failure. */
struct attribute *
get_attrib(struct list_head attrs, unsigned char *name);

#endif
