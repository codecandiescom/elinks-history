/* $Id: attrib.h,v 1.1 2002/12/25 00:09:06 pasky Exp $ */

#ifndef EL__USIVE_PARSER_ATTRIB_H
#define EL__USIVE_PARSER_ATTRIB_H

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

#endif
