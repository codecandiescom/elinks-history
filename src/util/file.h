/* $Id: file.h,v 1.10 2004/04/23 18:44:00 jonas Exp $ */

#ifndef EL__UTIL_FILE_H
#define EL__UTIL_FILE_H

#include <stdio.h>

int file_exists(const unsigned char *filename);
int file_can_read(const unsigned char *filename);

/* Strips all directory stuff from @filename and returns the
 * position of where the actual filename starts */
unsigned char *get_filename_position(unsigned char *filename);

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

#endif
