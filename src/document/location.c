/* Locations handling */
/* $Id: location.c,v 1.1 2002/03/28 22:26:11 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <links.h>

#include <document/session.h>
#include <document/view.h>

/* Yawn. Don't break crossdeps! */
#include <document/location.h>


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
