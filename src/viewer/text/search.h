/* $Id: search.h,v 1.1 2003/07/02 22:59:38 pasky Exp $ */

#ifndef EL__VIEWER_TEXT_SEARCH_H
#define EL__VIEWER_TEXT_SEARCH_H

#include "document/html/renderer.h"
#include "terminal/terminal.h"
#include "sched/session.h"

void draw_searched(struct terminal *term, struct f_data_c *scr);

void search_for(struct session *, unsigned char *);
void search_for_back(struct session *, unsigned char *);
void find_next(struct session *, struct f_data_c *, int);
void find_next_back(struct session *, struct f_data_c *, int);

#endif
