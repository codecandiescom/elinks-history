/* Get home directory */
/* $Id: home.c,v 1.3 2002/05/05 12:40:09 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <links.h>

#include <main.h>
#include <lowlevel/home.h>


unsigned char *links_home = NULL;
int first_use = 0;

unsigned char *
get_home(int *new)
{
	struct stat st;
	unsigned char *home = stracpy(getenv("HOME"));
	unsigned char *home_links;
	unsigned char *config_dir = stracpy(getenv("CONFIG_DIR"));

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

	home_links = stracpy(home);

	if (config_dir) {
		add_to_strn(&home_links, config_dir);

		while (home_links[0]
		       && dir_sep(home_links[strlen(home_links) - 1]))
			home_links[strlen(home_links) - 1] = 0;

		if (stat(home_links, &st) != -1 && S_ISDIR(st.st_mode)) {
			add_to_strn(&home_links, "/links");

	    	} else {
			fprintf(stderr, "CONFIG_DIR set to %s. But directory "
					"%s doesn't exist.\n\007",
				config_dir, home_links);
			sleep(3);
			mem_free(home_links);
			home_links = stracpy(home);
			add_to_strn(&home_links, ".links");
		}

		mem_free(config_dir);

	} else {
		add_to_strn(&home_links, ".links");
	}

	if (stat(home_links, &st)) {
		if (!mkdir(home_links, 0700))
			goto home_creat;
		if (config_dir)
			goto failed;
		goto first_failed;
	}

	if (S_ISDIR(st.st_mode))
		goto home_ok;

first_failed:
	mem_free(home_links);

	/* FIXME: home_links == NULL case --Zas */
	home_links = stracpy(home);
	add_to_strn(&home_links, "links");

	if (stat(home_links, &st)) {
		if (mkdir(home_links, 0700) == 0)
			goto home_creat;
		goto failed;
	}

	if (S_ISDIR(st.st_mode))
		goto home_ok;

failed:
	mem_free(home_links);
	mem_free(home);

	return NULL;

home_ok:
	if (new) *new = 0;

home_creat:
#if 0
	/* I've no idea if following is needed for newly created directories.
	 * It's bad thing to do it everytime. --pasky */
#ifdef HAVE_CHMOD
	chmod(home_links, 0700);
#endif
#endif
	add_to_strn(&home_links, "/");
	mem_free(home);

	return home_links;
}

void
init_home()
{
	links_home = get_home(&first_use);
	if (!links_home) {
		fprintf(stderr, "Unable to find or create links config "
				"directory. Please check, that you have $HOME "
				"variable set correctly and that you have "
				"write permission to your home directory."
				"\n\007");
		sleep(3);
		return;
	}
}

void
free_home()
{
	if (links_home) mem_free(links_home);
}
