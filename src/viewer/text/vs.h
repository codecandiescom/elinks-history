/* $Id: vs.h,v 1.23 2004/09/26 09:56:55 pasky Exp $ */

#ifndef EL__VIEWER_TEXT_VS_H
#define EL__VIEWER_TEXT_VS_H

/* Crossdeps are evil. */
struct document_view;
struct form_state;
struct session;
struct string_list_item;
struct uri;

struct view_state {
	struct document_view *doc_view;
	struct uri *uri;

	struct form_state *form_info;
	int form_info_len;

	int x, y;
	int current_link;

	int plain;
	unsigned int wrap:1;
	unsigned int did_fragment:1;

#ifdef CONFIG_ECMASCRIPT
	/* If set, we reset the interpreter state the next time we are going to
	 * render document attached to this view state. This means a real
	 * document (not just struct document_view, which randomly appears and
	 * disappears during gradual rendering) is getting replaced. So set this
	 * always when you replace the view_state URI, but also when reloading
	 * the document. You also cannot reset the document right away because
	 * it might take some time before the first rerendering is done and
	 * until then the old document is still hanging there. */
	int ecmascript_fragile:1;
	struct ecmascript_interpreter *ecmascript;
	/* This is a cross-rerenderings accumulator of
	 * @document.onload_snippets (see its description for juicy details).
	 * They enter this list as they continue to appear there, and they
	 * never leave it (so that we can always find from where to look for
	 * any new snippets in document.onload_snippets). Instead, as we
	 * go through the list we maintain a pointer to the last processed
	 * entry. */
	struct list_head onload_snippets; /* -> struct string_list_item */
	struct string_list_item *current_onload_snippet;
#endif
};

void init_vs(struct view_state *, struct uri *uri, int);
void destroy_vs(struct view_state *);

void copy_vs(struct view_state *, struct view_state *);
void check_vs(struct document_view *);

void next_frame(struct session *, int);

#endif
