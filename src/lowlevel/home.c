/* Get home directory */
/* $Id: home.c,v 1.28 2003/10/03 15:32:08 kuser Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "main.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "util/memory.h"
#include "util/string.h"


unsigned char *elinks_home = NULL;
int first_use = 0;

static inline void
strip_trailing_dir_sep(unsigned char *path)
{
	int i;

	for (i = strlen(path) - 1; i > 0; i--)
		if (!dir_sep(path[i]))
			break;
	
	path[i + 1] = 0;
}

static unsigned char *
test_confdir(unsigned char *confdir)
{
	struct stat st;

	if (stat(confdir, &st)) {
		if (!mkdir(confdir, 0700)) {
#if 0
		/* I've no idea if following is needed for newly created
		 * directories.  It's bad thing to do it everytime. --pasky */
#ifdef HAVE_CHMOD
			chmod(home_elinks, 0700);
#endif
#endif
			return confdir;
		}

		return NULL;
	}

	if (S_ISDIR(st.st_mode)) {
		first_use = 0;
		return confdir;
	}

	return NULL;
}

/* TODO: Check possibility to use <libgen.h> dirname. */
static unsigned char *
elinks_dirname(unsigned char *path)
{
	int i;
	unsigned char *dir;

	if (!path)
		return NULL;

	dir = stracpy(path);
	if (!dir)
		return NULL;

	for (i = strlen(dir) - 1; i >= 0; i--)
		if (dir_sep(dir[i]))
			break;

	dir[i + 1] = 0;

	return dir;
}

static unsigned char *
get_home(void)
{
	struct stat st;
	unsigned char *home_elinks;
	unsigned char *envhome = getenv("HOME");
	unsigned char *home = envhome ? stracpy(envhome)
				      : elinks_dirname(path_to_exe);
	unsigned char *envconfdir = getenv("ELINKS_CONFDIR");

	/* TODO: We want to use commandline option instead of environment
	 * variable, especially one with so common name. */

	if (!home) return NULL;

	strip_trailing_dir_sep(home);

	if (home[0]) add_to_strn(&home, "/");

	home_elinks = stracpy(home);
	if (!home_elinks) {
		mem_free(home);
		return NULL;
	}

	if (envconfdir) {
		add_to_strn(&home_elinks, envconfdir);

		strip_trailing_dir_sep(elinks_home);

		if (stat(home_elinks, &st) != -1 && S_ISDIR(st.st_mode)) {
			add_to_strn(&home_elinks, "/elinks");

	    	} else {
			error(gettext("ELINKS_CONFDIR set to %s, but "
				      "directory %s doesn't exist."),
			      envconfdir, home_elinks);
			sleep(3);
			mem_free(home_elinks);
			home_elinks = stracpy(home);
			add_to_strn(&home_elinks, ".elinks");
		}

	} else {
		add_to_strn(&home_elinks, ".elinks");
	}

	if (!test_confdir(home_elinks)) {
		mem_free(home_elinks);

		home_elinks = stracpy(home);
		if (!home_elinks) {
			mem_free(home);
			return NULL;
		}

		add_to_strn(&home_elinks, "elinks");

		if (!test_confdir(home_elinks)) {
			mem_free(home_elinks);
			mem_free(home);

			return NULL;
		}
	}

	add_to_strn(&home_elinks, "/");
	mem_free(home);

	return home_elinks;
}

void
init_home(void)
{
	first_use = 1;
	elinks_home = get_home();
	if (!elinks_home) {
		error(gettext("Unable to find or create ELinks config "
			      "directory. Please check if you have $HOME "
			      "variable set correctly and if you have "
			      "write permission to your home directory."));
		sleep(3);
		return;
	}
}

void
free_home(void)
{
	if (elinks_home) mem_free(elinks_home);
}
