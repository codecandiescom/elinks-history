/* $Id: refresh.h,v 1.4 2003/01/05 16:48:13 pasky Exp $ */

#ifndef EL__DIALOGS_REFRESH_H
#define EL__DIALOGS_REFRESH_H

#include "lowlevel/terminal.h"
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
