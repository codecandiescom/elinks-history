/* Locations handling */
/* $Id: location.c,v 1.2 2002/03/28 22:53:35 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <links.h>

#include <document/location.h>
#include <document/session.h>


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
