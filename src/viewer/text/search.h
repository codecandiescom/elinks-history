/* $Id: search.h,v 1.22 2005/04/07 11:32:25 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_SEARCH_H
#define EL__VIEWER_TEXT_SEARCH_H

#include "document/view.h"

struct module;
struct session;
struct terminal;

extern struct module search_history_module;

void draw_searched(struct terminal *term, struct document_view *doc_view);

enum frame_event_status find_next(struct session *ses, struct document_view *doc_view, int direction);

enum frame_event_status search_dlg(struct session *ses, struct document_view *doc_view, int direction);
enum frame_event_status search_typeahead(struct session *ses, struct document_view *doc_view, int direction);

static inline int has_search_word(struct document_view *doc_view)
{
	return (doc_view->search_word
		&& *doc_view->search_word
		&& (*doc_view->search_word)[0]);
}

#endif
