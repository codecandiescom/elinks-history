/* $Id: file.h,v 1.3 2003/01/13 11:18:15 zas Exp $ */

#ifndef EL__UTIL_FILE_H
#define EL__UTIL_FILE_H

#include <stdio.h>

int file_exists(const unsigned char *);
unsigned char *expand_tilde(unsigned char *);
unsigned char *get_unique_name(unsigned char *);
unsigned char *file_read_line(unsigned char *, size_t *, FILE *, int *);
unsigned char *safe_fgets(unsigned char *, int, FILE *);

#endif
