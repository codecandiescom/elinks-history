/* $Id: file.h,v 1.6 2003/06/08 14:39:57 jonas Exp $ */

#ifndef EL__UTIL_FILE_H
#define EL__UTIL_FILE_H

#include <stdio.h>

int file_exists(const unsigned char *filename);

/* Tilde is only expanded for the current users homedir (~/). */
/* The returned file name is allocated. */
unsigned char *expand_tilde(unsigned char *filename);

/* Generate a unique file name by trial and error based on the @fileprefix by
 * adding suffix counter (e.g. '.42'). */
/* The returned file name is allocated if @fileprefix is not unique. */
unsigned char *get_unique_name(unsigned char *fileprefix);

/* Read a line from @file into the dynamically allocated @line, increasing
 * @line if necessary. Ending whitespace is trimmed. If a line ends
 * with "\" the next line is read too. */
/* If @line is NULL the returned line is allocated and if file reading fails
 * @line is free()d. */
unsigned char *file_read_line(unsigned char *line, size_t *linesize,
			      FILE *file, int *linenumber);

/* Wrapper around fgets() that ensures that @buffer is NUL terminated. */
unsigned char *safe_fgets(unsigned char *buffer, int size, FILE *file);

#endif
