/* File utilities */
/* $Id: file.c,v 1.10 2003/04/28 09:40:27 zas Exp $ */

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

#include "elinks.h"

#include "util/error.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"

/* XXX: On some systems, fgets() won't put NUL at the end of
 * the string. -- Mikulas */
/* Which systems ??? --Zas */
inline unsigned char *
safe_fgets(unsigned char *s, int size, FILE *stream)
{
	unsigned char *ret = fgets(s, size, stream);

	if (ret) {
		unsigned char *p = memchr(s, '\n', size - 2);

		/* Ensure NUL termination. */
		if (p) *(++p) = '\0';
		else s[size - 1] = '\0';
	}

	return ret;
}

/* Only returns true/false. */
inline int
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
	unsigned char *file = init_str();
	int fl = 0;

	if (fi[0] == '~' && dir_sep(fi[1])) {
		unsigned char *home = getenv("HOME");

		if (home) {
			add_to_str(&file, &fl, home);
			fi++;
		}
	}

	add_to_str(&file, &fl, fi);
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

	/* This 'copy_string' is not really needed, but it's replacing a call
	 * to 'expand_tilde', so the rest of the code doesn't need to be touched.
	 * This function should be cleaned, anyway, to get rid of the 'mem_free'
	 * calls for 'prefix', etc... NOTE THAT NOW THIS FUNCTION WANTS 'fileprefix'
	 * already 'tildexpanded'. This is fixed in current uses of this function. */
	copy_string(&prefix, fileprefix);
	if (!prefix) return NULL;
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

	if (prefix != file) mem_free(prefix);
	return file;
}

/*
 * Read a line from ``file'' into the dynamically allocated ``line'',
 * increasing ``line'' if necessary. The ending "\n" or "\r\n" is removed.
 * If a line ends with "\", this char and the linefeed is removed,
 * and the next line is read too. Thanks Mutt.
 */

unsigned char *
file_read_line(unsigned char *line, size_t *size, FILE *file, int *lineno)
{
	size_t offset = 0;
	unsigned char *ch;

	if (!line) {
		line = mem_alloc(MAX_STR_LEN);
		*size = MAX_STR_LEN;
	}

	while (1) {
		if (safe_fgets(line + offset, *size - offset, file) == NULL) {
			if (line) mem_free(line);
			return NULL;
		}

		if ((ch = strchr(line + offset, '\n')) != NULL) {
			(*lineno)++;
			*ch = 0;
			if (ch > line && *(ch - 1) == '\r')
				*--ch = 0;
			if (ch == line || *(ch - 1) != '\\')
				return line;
			offset = ch - line - 1;
		} else {
			int c;
			/*
			 * This is kind of a hack. We want to know if the
			 * char at the current point in the input stream is EOF.
			 * feof() will only tell us if we've already hit EOF, not
			 * if the next character is EOF. So, we need to read in
			 * the next character and manually check if it is EOF.
			 */
			c = getc(file);
			if (c == EOF) {
				/* The last line of file isn't \n terminated */
				(*lineno)++;
				return line;
			} else {
				ungetc(c, file); /* undo our dammage */
				/* There wasn't room for the line -- increase ``line'' */
				offset = *size - 1; /* overwrite the terminating 0 */
				*size += MAX_STR_LEN;
				mem_realloc(line, *size);
			}
		}
	}
}
