/* Attributes utility tools */
/* $Id: attrib.c,v 1.2 2002/12/30 23:55:18 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "elusive/parser/attrib.h"
#include "util/lists.h"
#include "util/string.h"


struct attribute *
get_attrib(struct list_head attrs, unsigned char *name)
{
	struct attribute *attrib;

	foreach (attrib, attrs) {
		if (strncasecmp(name, attrib->name, attrib->namelen))
			continue;

		return attrib;
	}

	return NULL;
}

struct attribute *
add_attrib(struct list_head attrs, unsigned char *name, int namelen,
	   unsigned char *value, int valuelen)
{
	struct attribute *attrib = mem_calloc(1, sizeof(struct attribute));

	if (!attrib) return NULL;

	attrib->name = name, attrib->namelen = namelen;
	attrib->value = valuae, attrib->valuelen = valuelen;
	add_to_list(attrs, attrib);

	return attrib;
}
