/* $Id: select.h,v 1.14 2005/04/11 17:16:18 jonas Exp $ */

#ifndef EL__LOWLEVEL_SELECT_H
#define EL__LOWLEVEL_SELECT_H

#include "util/ttime.h"

long select_info(int);
void select_loop(void (*)(void));

int register_bottom_half_do(void (*)(void *), void *);
#define register_bottom_half(fn, data) \
	register_bottom_half_do((void (*)(void *))(fn), (void *)(data))

void check_bottom_halves(void);

#define H_READ	0
#define H_WRITE	1
#define H_ERROR	2
#define H_DATA	3

void *get_handler(int, int);
void set_handlers(int, void (*)(void *), void (*)(void *), void (*)(void *), void *);
#define clear_handlers(fd) set_handlers(fd, NULL, NULL, NULL, NULL)

int can_read(int fd);
int can_write(int fd);

#endif
