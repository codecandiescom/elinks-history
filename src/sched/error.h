/* $Id: error.h,v 1.5 2003/11/29 18:32:03 jonas Exp $ */

#ifndef EL__SCHED_ERROR_H
#define EL__SCHED_ERROR_H

struct terminal;

unsigned char *get_err_msg(int state, struct terminal *term);

void free_strerror_buf(void);

unsigned char *
get_stat_msg(struct download *stat, struct terminal *term,
	     int wide, int full, unsigned char *separator);

#endif
