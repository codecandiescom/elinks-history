/* Open in new window handling */
/* $Id: newwin.c,v 1.1 2003/10/27 23:22:11 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "osdep/os_dep.h"
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

struct {
	enum term_env_type env;
	void (*fn)(struct terminal *term, unsigned char *, unsigned char *);
	unsigned char *text;
} oinw[] = {
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

struct open_in_new *
get_open_in_new(int environment)
{
	int i;
	struct open_in_new *oin = NULL;
	int noin = 0;

	for (i = 0; oinw[i].env; i++) {
		struct open_in_new *x;

		if (!(environment & oinw[i].env))
			continue;

		x = mem_realloc(oin, (noin + 2) * sizeof(struct open_in_new));
		if (!x) continue;

		oin = x;
		oin[noin].text = oinw[i].text;
		oin[noin].fn = oinw[i].fn;
		noin++;
		oin[noin].text = NULL;
		oin[noin].fn = NULL;
	}

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
	struct open_in_new *oin = get_open_in_new(term->environment);

	if (!oin) return 0;
	if (!oin[1].text) {
		mem_free(oin);
		return 1;
	}
	mem_free(oin);
	return 2;
}
