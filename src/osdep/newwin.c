/* Open in new window handling */
/* $Id: newwin.c,v 1.7 2004/04/15 15:35:28 jonas Exp $ */

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


static void
exec_new_elinks(struct terminal *term, unsigned char *xterm,
		unsigned char *exe_name, unsigned char *param)
{
	unsigned char *str = straconcat(xterm, " ", exe_name, " ", param, NULL);

	if (!str) return;
	exec_on_terminal(term, str, "", 2);
	mem_free(str);
}

static void
open_in_new_twterm(struct terminal *term, unsigned char *exe_name,
		   unsigned char *param)
{
	unsigned char *twterm = getenv("ELINKS_TWTERM");

	if (!twterm) twterm = getenv("LINKS_TWTERM");
	if (!twterm) twterm = DEFAULT_TWTERM_CMD;
	exec_new_elinks(term, twterm, exe_name, param);
}

static void
open_in_new_xterm(struct terminal *term, unsigned char *exe_name,
		  unsigned char *param)
{
	unsigned char *xterm = getenv("ELINKS_XTERM");

	if (!xterm) xterm = getenv("LINKS_XTERM");
	if (!xterm) xterm = DEFAULT_XTERM_CMD;
	exec_new_elinks(term, xterm, exe_name, param);
}

static void
open_in_new_screen(struct terminal *term, unsigned char *exe_name,
		   unsigned char *param)
{
	exec_new_elinks(term, DEFAULT_SCREEN_CMD, exe_name, param);
}

struct open_in_new oinw[] = {
	{ENV_XWIN, open_in_new_xterm, N_("~Xterm")},
	{ENV_TWIN, open_in_new_twterm, N_("T~wterm")},
	{ENV_SCREEN, open_in_new_screen, N_("~Screen")},
#ifdef OS2
	{ENV_OS2VIO, open_in_new_vio, N_("~Window")},
	{ENV_OS2VIO, open_in_new_fullscreen, N_("~Full screen")},
#endif
#ifdef WIN32
	{ENV_WIN32, open_in_new_win32, N_("~Window")},
#endif
#ifdef BEOS
	{ENV_BE, open_in_new_be, N_("~BeOS terminal")},
#endif
	{0, NULL, NULL}
};

#define foreach_oinw(i, term_env) \
	for ((i) = 0; oinw[(i)].env; (i)++) if (!((term_env) & oinw[(i)].env))

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
