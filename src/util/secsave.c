/* Secure file saving handling */
/* $Id: secsave.c,v 1.4 2002/05/06 14:12:15 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#include <errno.h>

#include <links.h>

#include <config/options.h>
#include <util/secsave.h>


/* If secure_save is set:
 * ~~~~~~~~~~~~~~~~~~~~~
 *
 * A call to secure_open("/home/me/.links/filename", mask) will open a file
 * named "filename.tmp" in /home/me/.links/ and return a pointer to a structure
 * secure_save_info on success or NULL on error.
 * 
 * Note: if a file named "filename.tmp" exists, it will be truncated to a size
 * of zero.
 *
 * Subsequent write operations are done using returned secure_save_info FILE *
 * field named fp.
 *
 * If an error is encountered, secure_save_info int field named err is set
 * (automatically if using secure_fp*() functions or by programmer)
 *
 * When secure_close() is called, "filename.tmp" is closed, and if
 * secure_save_info err field has a value of zero, "filename.tmp" is renamed to
 * "filename".
 *
 * WARNING: since rename() is used, any symlink called "filename" is replaced
 * by a regular file.
 *
 * If secure_save is unset:
 * ~~~~~~~~~~~~~~~~~~~~~~~
 *
 * No temporary file is created, "filename" is truncated, all operations are
 * done on it, no rename occurs, symlinks are preserved.
 *
 * In both cases:
 * ~~~~~~~~~~~~~
 * 
 * Access rights are affected by secure_open() mask parameter.
 */


/* Open a file for writing in a secure way. It returns a pointer to a structure
 * secure_save_info on success, or NULL on failure. */
struct secure_save_info *
secure_open(unsigned char *file_name, mode_t mask)
{
	mode_t saved_mask;
	unsigned char *ext = NULL;
	struct secure_save_info *ssi = (struct secure_save_info *)
				       mem_alloc(sizeof(struct secure_save_info));

	if (!ssi) goto end;

	ssi->err = 0;

	ssi->file_name = stracpy(file_name);
	if (!ssi->file_name) goto free_f;

	if (secure_save) ext = ".tmp";
	ssi->tmp_file_name = straconcat(ssi->file_name, ext, NULL);
	if (!ssi->tmp_file_name) goto free_file_name;

	saved_mask = umask(mask);
	ssi->fp = fopen(ssi->tmp_file_name, "w");
	umask(saved_mask);

	if (ssi->fp) return ssi;

	mem_free(ssi->tmp_file_name);

free_file_name:
	mem_free(ssi->file_name);

free_f:
	mem_free(ssi);

end:
	return NULL;
}


/* Close a file opened with secure_open, and return 0 on success, errno
 * or -1 on failure. */
int
secure_close(struct secure_save_info *ssi)
{
	int ret = -1;

	if (fclose(ssi->fp) == EOF) {
		ret = errno;
		goto free;
	}
	if (ssi->err) {
		ret = ssi->err;
		goto free;
	}

	if (secure_save) {
		if (rename(ssi->tmp_file_name, ssi->file_name) == -1) {
			ret = errno;
		} else {
			/* Return 0 if file is successfully written. */
			ret = 0;
		}
	} else {
		ret = 0;
	}

free:
	mem_free(ssi->tmp_file_name);
	mem_free(ssi->file_name);
	mem_free(ssi);

	return ret;
}


/* fputs() wrapper, set ssi->err to errno on error. If ssi->err is set when
 * called, it immediatly returns EOF. */
int
secure_fputs(struct secure_save_info *ssi, const char *s)
{
	int ret;

	if (ssi->err) return EOF;
	
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

	if (ssi->err) return EOF;
	
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

	if (ssi->err) return -1;

	va_start(ap, format);
	ret = vfprintf(ssi->fp, format, ap);
	if (ret < 0) ssi->err = errno;
	va_end(ap);

	return ret;
}
