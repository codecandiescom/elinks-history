/* Some ELinks' auxiliary routines (ELinks<->gettext support) */
/* $Id: libintl.c,v 1.1 2003/01/03 01:25:48 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "src/intl/gettext/libintl.h"


unsigned char *system_language = NULL;
unsigned char *current_language = "system";

void
set_language(unsigned char *language)
{
	if (!system_language) {
		/* At this point current_language must be "system" yet. */
		system_language = getenv("LANGUAGE");
		if (!system_language) system_language = getenv("LC_ALL");
		if (!system_language) system_language = getenv("LC_MESSAGES");
		if (!system_language) system_language = getenv("LANG");
		if (!system_language) system_language = "en";
	}

	if (!strcmp(language, current_language)) {
		/* Nothing to do. */
		return;
	}

	if (!strcmp(language, "system")) {
		current_language = system_language;
	} else {
		current_language = language;
	}

	setenv("LANGUAGE", current_language, 1);

	/* Propagate the change to gettext. From the info manual. */
	{
		extern int _nl_msg_cat_cntr;

		_nl_msg_cat_cntr++;
	}
}
