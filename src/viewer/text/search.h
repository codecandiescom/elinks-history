/* $Id: search.h,v 1.19 2004/02/03 20:31:35 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_SEARCH_H
#define EL__VIEWER_TEXT_SEARCH_H

#include "document/view.h"

struct session;
struct terminal;

void draw_searched(struct terminal *term, struct document_view *doc_view);

void find_next(struct session *, struct document_view *doc_view, int);

void search_dlg(struct session *ses, struct document_view *doc_view, int a);
void search_typeahead(struct session *ses, struct document_view *doc_view, int a);

void init_search_history(void);
void done_search_history(void);

static inline int has_search_word(struct document_view *doc_view)
{
	return (doc_view->search_word
		&& *doc_view->search_word
		&& (*doc_view->search_word)[0]);
}

#endif
