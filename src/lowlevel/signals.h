/* Signals handling. */
/* $Id: signals.h,v 1.3 2003/05/25 09:41:44 zas Exp $ */

#ifndef EL__LOWLEVEL_SIGNALS_H
#define EL__LOWLEVEL_SIGNALS_H

#define NUM_SIGNALS	32

struct signal_handler {
	void (*fn)(void *);
	void *data;
	int critical;
};

extern int critical_section;

void install_signal_handler(int, void (*)(void *), void *, int);
void set_sigcld(void);
void sig_ctrl_c(struct terminal *t);
void clear_signal_mask_and_handlers(void);
void uninstall_alarm(void);
void handle_basic_signals(struct terminal *term);
void unhandle_terminal_signals(struct terminal *term);
int check_signals(void);

#endif /* EL__LOWLEVEL_SIGNALS_H */
