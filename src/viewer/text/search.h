/* $Id: search.h,v 1.4 2003/07/15 20:18:11 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_SEARCH_H
#define EL__VIEWER_TEXT_SEARCH_H

#include "sched/session.h"
#include "terminal/terminal.h"

void draw_searched(struct terminal *term, struct document_view *scr);

void search_for(struct session *, unsigned char *);
void search_for_back(struct session *, unsigned char *);
void find_next(struct session *, struct document_view *, int);
void find_next_back(struct session *, struct document_view *, int);

#endif
