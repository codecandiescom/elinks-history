/* Get system name */
/* $Id: sysname.c,v 1.1 2002/04/28 11:48:26 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <links.h>

#include <lowlevel/sysname.h>


unsigned char system_name[MAX_STR_LEN];

void
get_system_name()
{
	FILE *f;
	unsigned char *p;

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

fail:
	strcpy(system_name, SYSTEM_NAME);
}
