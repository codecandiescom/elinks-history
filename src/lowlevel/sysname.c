/* Get system name */
/* $Id: sysname.c,v 1.3 2002/05/10 13:06:10 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "links.h"

#include "lowlevel/sysname.h"


unsigned char system_name[MAX_STR_LEN];

void
get_system_name()
{
	FILE *f;
	unsigned char *p;

#ifdef HAVE_POPEN
	memset(system_name, 0, MAX_STR_LEN);

	f = popen("uname -srm", "r");
	if (!f) goto fail;

	if (fread(system_name, 1, MAX_STR_LEN - 1, f) <= 0) {
		pclose(f);
		goto fail;
	}

	pclose(f);

	for (p = system_name; *p; p++) {
		if (*p >= ' ')
			continue;
		*p = 0;
		break;
	}

	if (system_name[0])
		return;
#endif

fail:
	strcpy(system_name, SYSTEM_NAME);
}
