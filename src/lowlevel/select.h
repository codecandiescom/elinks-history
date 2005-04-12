/* $Id: select.h,v 1.15 2005/04/12 17:50:02 jonas Exp $ */

#ifndef EL__LOWLEVEL_SELECT_H
#define EL__LOWLEVEL_SELECT_H

#include "util/ttime.h"

long select_info(int);
void select_loop(void (*)(void));

typedef void (*select_handler_T)(void *);

int register_bottom_half_do(select_handler_T, void *);
#define register_bottom_half(fn, data) \
	register_bottom_half_do((select_handler_T) (fn), (void *) (data))

void check_bottom_halves(void);

#define H_READ	0
#define H_WRITE	1
#define H_ERROR	2
#define H_DATA	3

void *get_handler(int, int);
void set_handlers(int, select_handler_T, select_handler_T, select_handler_T, void *);
#define clear_handlers(fd) set_handlers(fd, NULL, NULL, NULL, NULL)

int can_read(int fd);
int can_write(int fd);

#endif
