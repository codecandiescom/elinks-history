/* Attributes utility tools */
/* $Id: attrib.c,v 1.1 2002/12/25 00:15:39 pasky Exp $ */

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
	struct attribute *attr;

	foreach (attr, attrs) {
		if (strncasecmp(name, attr->name, attr->namelen))
			continue;

		return attr;
	}

	return NULL;
}
