/* Locations handling */
/* $Id: location.c,v 1.11 2002/12/03 19:31:45 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "links.h"

#include "document/location.h"
#include "document/session.h"
#include "util/memory.h"
#include "util/string.h"


void
copy_location(struct location *dst, struct location *src)
{
	struct frame *f, *nf;

	dst->unhist_jump = src->unhist_jump;
	init_list(dst->frames);
	foreachback(f, src->frames) {
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
	foreach(frame, loc->frames) {
		destroy_vs(&frame->vs);
		mem_free(frame->name);
	}
	free_list(loc->frames);
	destroy_vs(&loc->vs);
	mem_free(loc);
}
