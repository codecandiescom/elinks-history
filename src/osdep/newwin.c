/* Open in new window handling */
/* $Id: newwin.c,v 1.13 2004/04/17 01:37:37 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "osdep/newwin.h"
#include "osdep/osdep.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/string.h"


static struct open_in_new oinw[] = {
	{ ENV_XWIN,	DEFAULT_XTERM_CMD,	    N_("~Xterm") },
	{ ENV_TWIN,	DEFAULT_TWTERM_CMD,	    N_("T~wterm") },
	{ ENV_SCREEN,	DEFAULT_SCREEN_CMD,	    N_("~Screen") },
#ifdef OS2
	{ ENV_OS2VIO,	DEFAULT_OS2_WINDOW_CMD,	    N_("~Window") },
	{ ENV_OS2VIO,	DEFAULT_OS2_FULLSCREEN_CMD, N_("~Full screen") },
#endif
#ifdef WIN32
	{ ENV_WIN32,	"",			    N_("~Window") },
#endif
#ifdef BEOS
	{ ENV_BE,	DEFAULT_BEOS_TERM_CMD,	    N_("~BeOS terminal") },
#endif
	{ 0, NULL, NULL }
};

#define foreach_oinw(i, term_env) \
	for ((i) = 0; oinw[(i)].env; (i)++) if (((term_env) & oinw[(i)].env))

struct open_in_new *
get_open_in_new(struct terminal *term)
{
	int i = can_open_in_new(term);
	size_t size = i ? (i + 1) * sizeof(struct open_in_new) : 0;
	struct open_in_new *oin = mem_calloc(1, size);
	int noin = 0;

	if (!oin) return NULL;

	foreach_oinw (i, term->environment)
		memcpy(&oin[noin++], &oinw[i], sizeof(struct open_in_new));

	return oin;
}

/* Returns:
 * 0 if it is impossible to open anything in anything new
 * 1 if there is one possible object capable of being spawn
 * >1 if there is >1 such available objects (it may not be the actual number of
 *    them, though) */
int
can_open_in_new(struct terminal *term)
{
	int i, possibilities = 0;

	foreach_oinw (i, term->environment)
		possibilities++;

	return possibilities;
}

void
open_new_window(struct terminal *term, unsigned char *exe_name,
		enum term_env_type environment, unsigned char *param)
{
	unsigned char *command = NULL;
	int i;

	foreach_oinw (i, environment)
		command = oinw[i].command;

	assert(command);

	if (environment & ENV_XWIN) {
		unsigned char *xterm = getenv("ELINKS_XTERM");

		if (!xterm) xterm = getenv("LINKS_XTERM");
		if (xterm) command = xterm;

	} else if (environment & ENV_TWIN) {
		unsigned char *twterm = getenv("ELINKS_TWTERM");

		if (!twterm) twterm = getenv("LINKS_TWTERM");
		if (twterm) command = twterm;
	}

	command = straconcat(command, " ", exe_name, " ", param, NULL);
	if (!command) return;

	exec_on_terminal(term, command, "", 2);
	mem_free(command);
}
