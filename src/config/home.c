/* Get home directory */
/* $Id: home.c,v 1.8 2002/06/17 07:42:31 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "links.h"

#include "main.h"
#include "lowlevel/home.h"
#include "util/memory.h"
#include "util/string.h"


unsigned char *elinks_home = NULL;
int first_use = 0;

unsigned char *
get_home(int *new)
{
	struct stat st;
	unsigned char *home = stracpy(getenv("HOME"));
	unsigned char *home_elinks;
	unsigned char *config_dir = stracpy(getenv("CONFIG_DIR"));

	/* TODO: We want to use commandline option instead of environment
	 * variable, especially one with so common name. */

	if (new) *new = 1;

	if (!home) {
		int i;

		home = stracpy(path_to_exe);
		if (!home) {
			if (config_dir) mem_free(config_dir);
			return NULL;
		}

		for (i = strlen(home) - 1; i >= 0; i--) {
			if (dir_sep(home[i])) {
				home[i + 1] = 0;
				break;
			}
		}

		if (i < 0) home[0] = 0;
	}

	while (home[0] && dir_sep(home[strlen(home) - 1]))
		home[strlen(home) - 1] = 0;

	if (home[0]) add_to_strn(&home, "/");

	home_elinks = stracpy(home);

	if (config_dir) {
		add_to_strn(&home_elinks, config_dir);

		while (home_elinks[0]
		       && dir_sep(home_elinks[strlen(home_elinks) - 1]))
			home_elinks[strlen(home_elinks) - 1] = 0;

		if (stat(home_elinks, &st) != -1 && S_ISDIR(st.st_mode)) {
			add_to_strn(&home_elinks, "/elinks");

	    	} else {
			fprintf(stderr, "CONFIG_DIR set to %s. But directory "
					"%s doesn't exist.\n\007",
				config_dir, home_elinks);
			sleep(3);
			mem_free(home_elinks);
			home_elinks = stracpy(home);
			add_to_strn(&home_elinks, ".elinks");
		}

		mem_free(config_dir);

	} else {
		add_to_strn(&home_elinks, ".elinks");
	}

	if (stat(home_elinks, &st)) {
		if (!mkdir(home_elinks, 0700))
			goto home_creat;
		if (config_dir)
			goto failed;
		goto first_failed;
	}

	if (S_ISDIR(st.st_mode))
		goto home_ok;

first_failed:
	mem_free(home_elinks);

	/* FIXME: home_elinks == NULL case --Zas */
	home_elinks = stracpy(home);
	add_to_strn(&home_elinks, "elinks");

	if (stat(home_elinks, &st)) {
		if (mkdir(home_elinks, 0700) == 0)
			goto home_creat;
		goto failed;
	}

	if (S_ISDIR(st.st_mode))
		goto home_ok;

failed:
	mem_free(home_elinks);
	mem_free(home);

	return NULL;

home_ok:
	if (new) *new = 0;

home_creat:
#if 0
	/* I've no idea if following is needed for newly created directories.
	 * It's bad thing to do it everytime. --pasky */
#ifdef HAVE_CHMOD
	chmod(home_elinks, 0700);
#endif
#endif
	add_to_strn(&home_elinks, "/");
	mem_free(home);

	return home_elinks;
}

void
init_home()
{
	elinks_home = get_home(&first_use);
	if (!elinks_home) {
		fprintf(stderr, "Unable to find or create ELinks config "
				"directory. Please check if you have $HOME "
				"variable set correctly and if you have "
				"write permission to your home directory."
				"\n\007");
		sleep(3);
		return;
	}
}

void
free_home()
{
	if (elinks_home) mem_free(elinks_home);
}
