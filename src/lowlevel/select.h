/* $Id: select.h,v 1.5 2003/05/23 21:22:20 zas Exp $ */

#ifndef EL__SELECT_H
#define EL__SELECT_H

#include "lowlevel/ttime.h"

extern int terminate;
extern struct list_head bottom_halves;

long select_info(int);
void select_loop(void (*)(void));
int register_bottom_half(void (*)(void *), void *);
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
