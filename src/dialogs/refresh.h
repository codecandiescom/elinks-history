/* $Id: refresh.h,v 1.6 2003/05/04 19:54:33 pasky Exp $ */

#ifndef EL__DIALOGS_REFRESH_H
#define EL__DIALOGS_REFRESH_H

#include "terminal/terminal.h"
#include "terminal/window.h"
#include "sched/session.h"

typedef void (*refresh_handler)(struct terminal *, void *, struct session *);

struct refresh {
	struct terminal *term;
	struct window *win;
	struct session *ses;
	refresh_handler fn;
	void *data;
	int timer;
};

void refresh_init(struct refresh *r, struct terminal *term,
		  struct session *ses, void *data, refresh_handler fn);

#endif
