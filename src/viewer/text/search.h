/* $Id: search.h,v 1.11 2003/12/01 15:11:20 pasky Exp $ */

#ifndef EL__VIEWER_TEXT_SEARCH_H
#define EL__VIEWER_TEXT_SEARCH_H

#include "document/view.h"

struct session;

void draw_searched(struct terminal *term, struct document_view *doc_view);

void search_for(struct session *, unsigned char *);
void search_for_back(struct session *, unsigned char *);
void find_next(struct session *, struct document_view *doc_view, int);
void find_next_back(struct session *, struct document_view *doc_view, int);

void search_dlg(struct session *ses, struct document_view *doc_view, int a);
void search_back_dlg(struct session *ses, struct document_view *doc_view, int a);

void init_search_history(void);
void done_search_history(void);

static inline int has_search_word(struct document_view *doc_view)
{
	return (doc_view->search_word
		&& *doc_view->search_word
		&& (*doc_view->search_word)[0]);
}

#endif
