/* Secure file saving handling */
/* $Id: secsave.h,v 1.3 2002/05/17 15:42:00 pasky Exp $ */

#ifndef EL__UTIL_SECFILE_H
#define EL__UTIL_SECFILE_H

#include <stdio.h>
#include <sys/types.h> /* mode_t */

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
