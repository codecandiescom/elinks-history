/* Periodic refresh of dialogs */
/* $Id: refresh.c,v 1.8 2002/12/08 21:01:20 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "dialogs/refresh.h"
#include "document/session.h"
#include "lowlevel/select.h"
#include "lowlevel/terminal.h"
#include "util/memory.h"


static void
refresh(struct refresh *r)
{
	struct refresh rr;

	r->timer = -1;
	memcpy(&rr, r, sizeof(struct refresh));
	delete_window(r->win);
	rr.fn(rr.term, rr.data, rr.ses);
}

static void
refresh_end(struct refresh *r)
{
	if (r->timer != -1) kill_timer(r->timer);
	mem_free(r);
}

static void
refresh_abort(struct dialog_data *dlg)
{
	refresh_end(dlg->dlg->udata2);
}

void
refresh_init(struct refresh *r, struct terminal *term,
	     struct session *ses, void *data, refresh_handler fn)
{
	r->term = term;
	r->win = term->windows.next;
	r->ses = ses;
	r->fn = fn;
	r->data = data;

	((struct dialog_data *) r->win->data)->dlg->abort = refresh_abort;
	r->timer = install_timer(RESOURCE_INFO_REFRESH, (void (*)(void *)) refresh, r);
}
