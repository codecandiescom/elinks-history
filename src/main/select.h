/* $Id: select.h,v 1.12 2005/03/03 15:49:43 zas Exp $ */

#ifndef EL__LOWLEVEL_SELECT_H
#define EL__LOWLEVEL_SELECT_H

#include "util/lists.h"
#include "util/ttime.h"

long select_info(int);
void select_loop(void (*)(void));

int register_bottom_half_do(void (*)(void *), void *);
#define register_bottom_half(fn, data) \
	register_bottom_half_do((void (*)(void *))(fn), (void *)(data))

void check_bottom_halves(void);

int install_timer(ttime, void (*)(void *), void *);
void kill_timer(int);

#define H_READ	0
#define H_WRITE	1
#define H_ERROR	2
#define H_DATA	3

void *get_handler(int, int);
void set_handlers(int, void (*)(void *), void (*)(void *), void (*)(void *), void *);

int can_read(int fd);
int can_write(int fd);

#endif
