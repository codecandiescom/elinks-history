/* $Id: error.h,v 1.1 2003/06/07 14:37:17 pasky Exp $ */

#ifndef EL__SCHED_ERROR_H
#define EL__SCHED_ERROR_H

unsigned char *get_err_msg(int state);

void free_strerror_buf(void);

#endif
