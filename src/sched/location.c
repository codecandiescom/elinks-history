/* Locations handling */
/* $Id: location.c,v 1.11 2004/03/22 14:35:40 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "sched/location.h"
#include "sched/session.h"
#include "util/memory.h"
#include "util/string.h"


void
copy_location(struct location *dst, struct location *src)
{
	struct frame *frame, *new_frame;

	init_list(dst->frames);
	foreachback (frame, src->frames) {
		/* One byte is reserved in struct vs. */
		new_frame = mem_alloc(sizeof(struct frame) + frame->vs.url_len);
		if (new_frame) {
			new_frame->name = stracpy(frame->name);
			if (!new_frame->name) {
				mem_free(new_frame);
				return;
			}
			new_frame->redirect_cnt = 0;
			copy_vs(&new_frame->vs, &frame->vs);
			add_to_list(dst->frames, new_frame);
		}
	}
	copy_vs(&dst->vs, &src->vs);
}

void
destroy_location(struct location *loc)
{
	struct frame *frame;

	foreach (frame, loc->frames) {
		destroy_vs(&frame->vs);
		mem_free(frame->name);
	}
	free_list(loc->frames);
	destroy_vs(&loc->vs);
	mem_free(loc);
}
