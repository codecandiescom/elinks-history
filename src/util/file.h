/* $Id: file.h,v 1.1 2002/09/13 20:29:36 pasky Exp $ */

#ifndef EL__UTIL_FILE_H
#define EL__UTIL_FILE_H

int file_exists(const unsigned char *);
unsigned char *expand_tilde(unsigned char *);
unsigned char *get_unique_name(unsigned char *);

#endif
