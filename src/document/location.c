/* Locations handling */
/* $Id: location.c,v 1.3 2002/03/28 22:59:17 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <links.h>

#include <document/location.h>
#include <document/session.h>


void
copy_location(struct location *dst, struct location *src)
{
	struct frame *f, *nf;

	init_list(dst->frames);
	foreachback(f, src->frames) {
		nf = mem_alloc(sizeof(struct frame) + strlen(f->vs.url) + 1);
		if (nf) {
			nf->name = stracpy(f->name);
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
	foreach(frame, loc->frames) {
		destroy_vs(&frame->vs);
		mem_free(frame->name);
	}
	free_list(loc->frames);
	destroy_vs(&loc->vs);
	mem_free(loc);
}
