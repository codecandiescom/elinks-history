/* File utilities */
/* $Id: file.c,v 1.21 2004/04/23 13:47:58 pasky Exp $ */

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

#include "util/conv.h"
#include "util/error.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"


/* Not that these two would be so useful for portability (they are ANSI C) but
 * they encapsulate the lowlevel stuff (need for <unistd.h>) nicely. */

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

int
file_can_read(const unsigned char *filename)
{
#ifdef HAVE_ACCESS
	return access(filename, R_OK) >= 0;
#else
	FILE *f = fopen(filename, "r");
	int ok = !!f;

	if (f) fclose(f);
	return ok;
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
				INTERNAL("Too big suffix in get_unique_name().");
			memtrigger *= 10;
			digits++;

			if (file != fileprefix) mem_free(file);
			file = mem_alloc(fileprefixlen + 2 + digits);
			if (!file) return NULL;

			memcpy(file, fileprefix, fileprefixlen);
			file[fileprefixlen] = '.';
		}

		longcat(&file[fileprefixlen + 1], NULL, suffix, digits + 1, 0);
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

	while (fgets(line + offset, *size - offset, file)) {
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

	mem_free_if(line);
	return NULL;
}
