/* Locations handling */
/* $Id: location.c,v 1.4 2003/06/15 14:05:11 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "sched/history.h"
#include "sched/location.h"
#include "sched/session.h"
#include "util/memory.h"
#include "util/string.h"


void
copy_location(struct location *dst, struct location *src)
{
	struct frame *f, *nf;

	init_list(dst->frames);
	foreachback (f, src->frames) {
		nf = mem_alloc(sizeof(struct frame) + strlen(f->vs.url) + 1);
		if (nf) {
			nf->name = stracpy(f->name);
			if (!nf->name) {
				mem_free(nf);
				return;
			}
			nf->redirect_cnt = 0;
			copy_vs(&nf->vs, &f->vs);
			add_to_list(dst->frames, nf);
		}
	}
	copy_vs(&dst->vs, &src->vs);
}

void
destroy_location(struct location *loc)
{
	struct frame *frame;

	del_from_list(loc);
	foreach (frame, loc->frames) {
		destroy_vs(&frame->vs);
		mem_free(frame->name);
	}
	free_list(loc->frames);
	destroy_vs(&loc->vs);
	mem_free(loc);
}

void
set_session_location(struct session *ses, struct location *loc, int direction)
{
	assert(!ses->location);

	add_to_history(ses, loc);
#if 0
	if (have_location(ses)) {
		if (direction < 0)
			add_to_history(ses, ses->location);
		else
			add_to_unhistory(ses, ses->location);
	}

	/* Just to be sure not to have any loose ends */
	init_list(*loc);
	ses->location = loc;
#endif
}
