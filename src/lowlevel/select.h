/* $Id: select.h,v 1.3 2002/05/08 13:55:04 pasky Exp $ */

#ifndef EL__SELECT_H
#define EL__SELECT_H

#include "lowlevel/ttime.h"

extern int terminate;

long select_info(int);
void select_loop(void (*)());
int register_bottom_half(void (*)(void *), void *);
void check_bottom_halves();
int install_timer(ttime, void (*)(void *), void *);
void kill_timer(int);

#define H_READ	0
#define H_WRITE	1
#define H_ERROR	2
#define H_DATA	3

void *get_handler(int, int);
void set_handlers(int, void (*)(void *), void (*)(void *), void (*)(void *), void *);
void install_signal_handler(int, void (*)(void *), void *, int);
void set_sigcld();

int can_read(int fd);
int can_write(int fd);

#endif
