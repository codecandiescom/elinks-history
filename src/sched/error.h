/* $Id: error.h,v 1.2 2003/06/07 14:56:07 pasky Exp $ */

#ifndef EL__SCHED_ERROR_H
#define EL__SCHED_ERROR_H

struct terminal;

unsigned char *get_err_msg(int state, struct terminal *term);

void free_strerror_buf(void);

#endif
