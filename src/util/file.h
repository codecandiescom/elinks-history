/* $Id: file.h,v 1.2 2002/12/10 20:58:47 pasky Exp $ */

#ifndef EL__UTIL_FILE_H
#define EL__UTIL_FILE_H

#include <stdio.h>

int file_exists(const unsigned char *);
unsigned char *expand_tilde(unsigned char *);
unsigned char *get_unique_name(unsigned char *);
unsigned char *file_read_line(unsigned char *, size_t *, FILE *, int *);

#endif
