/* Secure file saving handling */
/* $Id: secsave.h,v 1.4 2003/06/04 17:05:43 zas Exp $ */

#ifndef EL__UTIL_SECFILE_H
#define EL__UTIL_SECFILE_H

#include <stdio.h>
#include <sys/types.h> /* mode_t */

enum secsave_errno_set {
	NONE = 0,
	DISABLED = 1, /* secsave is disabled. */
	OUT_OF_MEM = 2, /* memory allocation failure */
	OTHER = 3, /* see err field in struct secure_save_info */
};

extern enum secsave_errno_set secsave_errno; /* internal secsave error number */

struct secure_save_info {
	FILE *fp; /* file stream pointer */
	unsigned char *file_name; /* final file name */
	unsigned char *tmp_file_name; /* temporary file name */
	int err; /* set to non-zero value in case of error */
	int secure_save; /* use secure save for this file */
};

struct secure_save_info *secure_open(unsigned char *, mode_t);
int secure_close(struct secure_save_info *);

int secure_fputs(struct secure_save_info *, const char *);
int secure_fputc(struct secure_save_info *, int);

int secure_fprintf(struct secure_save_info *, const char *, ...);

#endif
