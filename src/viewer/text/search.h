/* $Id: search.h,v 1.6 2003/10/05 20:14:00 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_SEARCH_H
#define EL__VIEWER_TEXT_SEARCH_H

#include "bfu/inphist.h"
#include "sched/session.h"
#include "terminal/terminal.h"

void draw_searched(struct terminal *term, struct document_view *scr);

void search_for(struct session *, unsigned char *);
void search_for_back(struct session *, unsigned char *);
void find_next(struct session *, struct document_view *, int);
void find_next_back(struct session *, struct document_view *, int);

extern struct input_history search_history;

void search_dlg(struct session *ses, struct document_view *f, int a);
void search_back_dlg(struct session *ses, struct document_view *f, int a);

void load_search_history(void);
void save_search_history(void);

#endif
