/* Secure file saving handling */
/* $Id: secsave.c,v 1.19 2002/09/17 14:40:19 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "links.h"

#include "config/options.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"


/* If secure_file_saving is set:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * A call to secure_open("/home/me/.elinks/filename", mask) will open a file
 * named "filename.tmp_XXXXXX" in /home/me/.elinks/ and return a pointer to a
 * structure secure_save_info on success or NULL on error.
 *
 * filename.tmp_XXXXXX can't conflict with any file since it's created using
 * mkstemp(). XXXXXX is a random string.
 *
 * Subsequent write operations are done using returned secure_save_info FILE *
 * field named fp.
 *
 * If an error is encountered, secure_save_info int field named err is set
 * (automatically if using secure_fp*() functions or by programmer)
 *
 * When secure_close() is called, "filename.tmp_XXXXXX" is closed, and if
 * secure_save_info err field has a value of zero, "filename.tmp_XXXXXX" is
 * renamed to "filename".
 *
 * WARNING: since rename() is used, any symlink called "filename" may be
 * replaced by a regular file. If destination file isn't a regular file,
 * then secsave is disabled for that file.
 *
 * If secure_file_saving is unset:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * No temporary file is created, "filename" is truncated, all operations are
 * done on it, no rename occurs, symlinks are preserved.
 *
 * In both cases:
 * ~~~~~~~~~~~~~
 *
 * Access rights are affected by secure_open() mask parameter.
 */

/* FIXME: locking system on files about to be rewritten ? */
/* FIXME: Low risk race conditions about ssi->file_name. */


/* Open a file for writing in a secure way. It returns a pointer to a structure
 * secure_save_info on success, or NULL on failure. */
struct secure_save_info *
secure_open(unsigned char *file_name, mode_t mask)
{
	mode_t saved_mask;
	struct stat st;
	struct secure_save_info *ssi = (struct secure_save_info *)
				       mem_calloc(1, sizeof(struct secure_save_info));

	if (!ssi) goto end;

	ssi->secure_save = get_opt_int("secure_file_saving");

	ssi->file_name = stracpy(file_name);
	if (!ssi->file_name) goto free_f;

	/* Check properties of final file. */
#ifdef FS_UNIX_SOFTLINKS
	if (lstat(ssi->file_name, &st)) {
#else
	if (stat(ssi->file_name, &st)) {
#endif
		/* We ignore error caused by file inexistence. */
		if (errno != ENOENT) {
			/* lstat() error. */
			ssi->err = errno;
			goto free_file_name;
		}
	} else {
		if (!S_ISREG(st.st_mode)) {
			/* Not a regular file, secure_save is disabled. */
			ssi->secure_save = 0;
		} else {
#ifdef HAVE_ACCESS
			/* XXX: access() do not work with setuid programs. */
			if (access(ssi->file_name, R_OK | W_OK) < 0) {
				ssi->err = errno;
				goto free_file_name;
			}
#else
			FILE *f1;

			/* We still have a race condition here between
			 * [l]stat() and fopen() */

			f1 = fopen(ssi->file_name, "r+");
			if (f1) {
				fclose(f1);
			} else {
				ssi->err = errno;
				goto free_file_name;
			}
#endif
		}
	}

	saved_mask = umask(mask);

	if (ssi->secure_save) {
		/* We use a random name for temporary file, mkstemp() opens
		 * the file and return a file descriptor named fd, which is
		 * then converted to FILE * using fdopen().
		 */
		int fd;
		unsigned char *randname = straconcat(ssi->file_name,
						     ".tmp_XXXXXX", NULL);

		if (!randname) goto free_file_name;

		fd = mkstemp(randname);
		if (fd == -1) {
			mem_free(randname);
			goto free_file_name;
		}

		ssi->fp = fdopen(fd, "w");
		if (!ssi->fp) {
			ssi->err = errno;
			mem_free(randname);
			goto free_file_name;
		}

		ssi->tmp_file_name = randname;
	} else {
		/* No need to create a temporary file here. */
		ssi->fp = fopen(ssi->file_name, "w");
		if (!ssi->fp) {
			ssi->err = errno;
			goto free_file_name;
		}
	}

	umask(saved_mask);

	return ssi;

free_file_name:
	mem_free(ssi->file_name);
	ssi->file_name = NULL;

free_f:
	mem_free(ssi);
	ssi = NULL;

end:
	return NULL;
}


/* Close a file opened with secure_open, and return 0 on success, errno
 * or -1 on failure. */
int
secure_close(struct secure_save_info *ssi)
{
	int ret = -1;

	if (!ssi) return ret;
	if (!ssi->fp) goto free;

	if (fclose(ssi->fp) == EOF) {
		ret = errno;
		goto free;
	}
	if (ssi->err) {
		ret = ssi->err;
		goto free;
	}

	if (ssi->secure_save) {
		if (ssi->file_name && ssi->tmp_file_name) {
#ifdef OS2
			/* OS/2 needs this, however it breaks atomicity on
			 * UN*X. */
			unlink(ssi->file_name);
#endif
			/* FIXME: Race condition on ssi->file_name. The file
			 * named ssi->file_name may have changed since
			 * secure_open() call (where we stat() file and
			 * more..).  */
			if (rename(ssi->tmp_file_name, ssi->file_name) == -1) {
				ret = errno;
			} else {
				/* Return 0 if file is successfully written. */
				ret = 0;
			}
		}
	} else {
		ret = 0;
	}

free:
	if (ssi->tmp_file_name) mem_free(ssi->tmp_file_name);
	if (ssi->file_name) mem_free(ssi->file_name);
	if (ssi) mem_free(ssi);

	return ret;
}


/* fputs() wrapper, set ssi->err to errno on error. If ssi->err is set when
 * called, it immediatly returns EOF. */
int
secure_fputs(struct secure_save_info *ssi, const char *s)
{
	int ret;

	if (!ssi || !ssi->fp || ssi->err) return EOF;

	ret = fputs(s, ssi->fp);
	if (ret == EOF) ssi->err = errno;

	return ret;
}


/* fputc() wrapper, set ssi->err to errno on error. If ssi->err is set when
 * called, it immediatly returns EOF. */
int
secure_fputc(struct secure_save_info *ssi, int c)
{
	int ret;

	if (!ssi || !ssi->fp || ssi->err) return EOF;

	ret = fputc(c, ssi->fp);
	if (ret == EOF) ssi->err = errno;

	return ret;
}

/* fprintf() wrapper, set ssi->err to errno on error and return a negative
 * value. If ssi->err is set when called, it immediatly returns -1. */
int
secure_fprintf(struct secure_save_info *ssi, const char *format, ...)
{
	va_list ap;
	int ret;

	if (!ssi || !ssi->fp || ssi->err) return -1;

	va_start(ap, format);
	ret = vfprintf(ssi->fp, format, ap);
	if (ret < 0) ssi->err = errno;
	va_end(ap);

	return ret;
}
