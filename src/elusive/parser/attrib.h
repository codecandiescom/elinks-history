/* $Id: attrib.h,v 1.2 2002/12/25 00:15:39 pasky Exp $ */

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
	unsigned char *sval; int svallen;

	enum attribute_type type;
	void *value;
};

/* Get attribute of a given name from the supplied list. Returns NULL on
 * failure. */
struct attribute *
get_attrib(struct list_head attrs, unsigned char *name);

#endif
