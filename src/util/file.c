/* File utilities */
/* $Id: file.c,v 1.16 2003/07/21 22:01:06 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
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
expand_tilde(unsigned char *filename)
{
	struct string file;

	if (!init_string(&file)) return NULL;

	if (filename[0] == '~' && (!filename[1] || dir_sep(filename[1]))) {
		unsigned char *home = getenv("HOME");

		if (home) {
			add_to_string(&file, home);
			filename++;
		}
	}

	add_to_string(&file, filename);

	return file.source;
}

unsigned char *
get_unique_name(unsigned char *fileprefix)
{
	unsigned char *file = fileprefix;
	int fileprefixlen = strlen(fileprefix);
	int memtrigger = 1;
	int suffix = 1;
	int digits = 0;

	while (file_exists(file)) {
		if (!(suffix < memtrigger)) {
			if (suffix >= 10000)
				internal("Too big suffix in get_unique_name().");
			memtrigger *= 10;
			digits++;

			if (file != fileprefix) mem_free(file);
			file = mem_alloc(fileprefixlen + 2 + digits);
			if (!file) return NULL;

			safe_strncpy(file, fileprefix, fileprefixlen + 1);
		}

		sprintf(&file[fileprefixlen], ".%d", suffix);
		suffix++;
	}

	return file;
}

unsigned char *
file_read_line(unsigned char *line, size_t *size, FILE *file, int *lineno)
{
	size_t offset = 0;

	if (!line) {
		line = mem_alloc(MAX_STR_LEN);
		if (!line)
			return NULL;

		*size = MAX_STR_LEN;
	}

	while (safe_fgets(line + offset, *size - offset, file)) {
		unsigned char *linepos = strchr(line + offset, '\n');

		if (!linepos) {
			/* Test if the line buffer should be increase because
			 * it was continued and could not fit. */
			unsigned char *newline;
			int next = getc(file);

			if (next == EOF) {
				/* We are on the last line. */
				(*lineno)++;
				return line;
			}

			/* Undo our dammage */
			ungetc(next, file);
			offset = *size - 1;
			*size += MAX_STR_LEN;

			newline = mem_realloc(line, *size);
			if (!newline)
				break;
			line = newline;
			continue;
		}

		/* A whole line was read. Fetch next into the buffer if
		 * the line is 'continued'. */
		(*lineno)++;

		while (line < linepos && isspace(*linepos))
			linepos--;

		if (*linepos != '\\') {
			*(linepos + 1) = '\0';
			return line;
		}
		offset = linepos - line - 1;
	}

	if (line) mem_free(line);
	return NULL;
}
