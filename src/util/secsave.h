/* Secure file saving handling */
/* $Id: secsave.h,v 1.6 2003/06/04 17:15:14 pasky Exp $ */

#ifndef EL__UTIL_SECFILE_H
#define EL__UTIL_SECFILE_H

#include <stdio.h>
#include <sys/types.h> /* mode_t */

enum secsave_errno {
	SS_ERR_NONE,
	SS_ERR_DISABLED, /* secsave is disabled. */
	SS_ERR_OUT_OF_MEM, /* memory allocation failure */
	SS_ERR_OTHER, /* see err field in struct secure_save_info */
};

extern enum secsave_errno secsave_errno; /* internal secsave error number */

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
