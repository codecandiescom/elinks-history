/* $Id: select.h,v 1.9 2004/04/14 02:35:02 jonas Exp $ */

#ifndef EL__LOWLEVEL_SELECT_H
#define EL__LOWLEVEL_SELECT_H

#include "util/lists.h"
#include "util/ttime.h"

extern int terminate;
extern struct list_head bottom_halves;

long select_info(int);
void select_loop(void (*)(void));

int register_bottom_half(void (*)(void *), void *);
void do_check_bottom_halves(void);
#define check_bottom_halves() do { if (!list_empty(bottom_halves)) do_check_bottom_halves(); } while (0)

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
