/* File utilities */ 
/* $Id: file.c,v 1.1 2002/09/13 20:29:36 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef HAVE_ACCESS
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h> 
#endif

#include "links.h"

#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* Only returns true/false. */
int
file_exists(const unsigned char *filename)
{
#ifdef HAVE_ACCESS
	return access(filename, F_OK) >= 0;
#else
	struct stat buf;
	return stat (filename, &buf) >= 0;
#endif
}

unsigned char *
expand_tilde(unsigned char *fi)
{
	unsigned char *file = fi;

	if (file[0] == '~' && dir_sep(file[1])) {
		unsigned char *home = getenv("HOME");

		if (home) {
			int fl = 0;

			file = init_str();
			if (file) {
				add_to_str(&file, &fl, home);
				add_to_str(&file, &fl, fi + 1);
			} 
		}
	}

	return file;
}

/* Return unique file name based on a prefix by adding suffix counter. */
unsigned char *
get_unique_name(unsigned char *fileprefix)
{
	unsigned char *prefix;
	unsigned char *file;
	int memtrigger = 1;
	int suffix = 1;
	int digits = 0;
	int prefixlen;

	prefix = expand_tilde(fileprefix);
	prefixlen = strlen(prefix);
	file = prefix;

	while (file_exists(file)) {
		if (!(suffix < memtrigger)) {
			if (suffix >= 10000)
				internal("Too big suffix in get_unique_name().");
			memtrigger *= 10;
			digits++;

			if (file != prefix) mem_free(file);
			file = mem_alloc(prefixlen + 2 + digits);
			if (!file) return prefix;
			safe_strncpy(file, prefix, prefixlen + 1);
		}

		sprintf(&file[prefixlen], ".%d", suffix);
		suffix++;
	}

	if (prefix != file && prefix != fileprefix) mem_free(prefix);
	return file;
}
