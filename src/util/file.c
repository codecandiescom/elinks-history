/* File utilities */
/* $Id: file.c,v 1.31 2004/07/02 12:22:36 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "osdep/osdep.h"
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

	return stat(filename, &buf) >= 0;
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

/* Returns non-zero if filename is a directory, 0 else. */
int
file_is_dir(const unsigned char *filename)
{
	struct stat st;

	if (stat(filename, &st))
		return 0;

	return S_ISDIR(st.st_mode);
}

unsigned char *
get_filename_position(unsigned char *filename)
{
	unsigned char *pos;

	assert(filename);
	if_assert_failed return NULL;

	for (pos = filename; *pos; pos++)
		if (dir_sep(*pos)) filename = pos + 1;

	return filename;
}

unsigned char *
expand_tilde(unsigned char *filename)
{
	struct string file;

	if (!init_string(&file)) return NULL;

	if (filename[0] == '~') {
		if (!filename[1] || dir_sep(filename[1])) {
			unsigned char *home = getenv("HOME");

			if (home) {
				add_to_string(&file, home);
				filename++;
			}
#ifdef HAVE_GETPWNAM
		} else {
			struct passwd *passwd = NULL;
			unsigned char *user = filename + 1;
			int userlen = 0;

			while (user[userlen] && !dir_sep(user[userlen]))
				userlen++;

			user = memacpy(user, userlen);
			if (user) {
				passwd = getpwnam(user);
				mem_free(user);
			}

			if (passwd && passwd->pw_dir) {
				add_to_string(&file, passwd->pw_dir);
				filename += 1 + userlen;
			}
#endif
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
get_tempdir_filename(unsigned char *name)
{
	unsigned char *tmpdir = getenv("TMPDIR");

	if (!tmpdir || !*tmpdir) tmpdir = getenv("TMP");
	if (!tmpdir || !*tmpdir) tmpdir = getenv("TEMPDIR");
	if (!tmpdir || !*tmpdir) tmpdir = getenv("TEMP");
	if (!tmpdir || !*tmpdir) tmpdir = "/tmp";

	return straconcat(tmpdir, "/", name, NULL);
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


/* Some mkstemp() implementations do not set safe permissions,
 * so it may result in temporary files readable by all users.
 * This may be a problem when textarea fields are edited through
 * an external editor (see src/viewer/text/textarea.c).
 *
 * From 2001-12-23 mkstemp(3) gnu man page:
 *
 * ...
 * The file is then created with mode read/write and permissions 0666
 * (glibc  2.0.6 and  earlier), 0600 (glibc 2.0.7 and later).
 * ...
 *
 * NOTES:
 * The old behaviour (creating a file with mode 0666) may be a security
 * risk, especially since other Unix flavours use 0600, and somebody
 * might overlook this detail when porting programs.
 * More generally, the POSIX specification does not say anything
 * about file modes, so the application should make sure its umask is
 * set appropriately before calling mkstemp.
 */
int
safe_mkstemp(unsigned char *template)
{
	mode_t saved_mask = umask(0177);
	int fd = mkstemp(template);

	umask(saved_mask);

	return fd;
}
