/* Get system name */
/* $Id: sysname.c,v 1.7 2002/11/27 10:15:39 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#include "links.h"

#include "lowlevel/sysname.h"
#include "util/memory.h"
#include "util/string.h"


unsigned char system_name[MAX_STR_LEN];

void
get_system_name()
{
	FILE *f;
	unsigned char *p;

#if defined(HAVE_SYS_UTSNAME_H) && defined(HAVE_UNAME)
	struct utsname name;

	memset(&name, 0, sizeof(struct utsname));
	if (!uname(&name)) {
		unsigned char *str = init_str();
		int l = 0;

		add_to_str(&str, &l, name.sysname);
		add_to_str(&str, &l, " ");
		add_to_str(&str, &l, name.release);
		add_to_str(&str, &l, " ");
		add_to_str(&str, &l, name.machine);
		safe_strncpy(system_name, str, sizeof(system_name));
		mem_free(str);
		return;
	}
#endif

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
	safe_strncpy(system_name, SYSTEM_NAME, sizeof(system_name));
}
