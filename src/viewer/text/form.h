/* $Id: form.h,v 1.46 2004/12/19 00:17:55 pasky Exp $ */

#ifndef EL__VIEWER_TEXT_FORM_H
#define EL__VIEWER_TEXT_FORM_H

#include "document/forms.h"
#include "util/lists.h" /* LIST_HEAD */

struct document;
struct document_view;
struct link;
struct session;
struct term_event;
struct terminal;

/* This struct looks a little embarrassing, yeah. */
struct form_view {
	LIST_HEAD(struct form_view);

	/* XXX: This is actually very broken. lifespan of form_view (and
	 * clones, see copy_vs()) can get much longer than form's lifespan.
	 * That's why find_form_state() is so clumsy. So this way of
	 * associating form_view to form simply leads to frequent crashes,
	 * plain and simple. --pasky */
	struct form *form;

#ifdef CONFIG_ECMASCRIPT
	/* This holds the ECMAScript object attached to this structure. It can
	 * be NULL since the object is created on-demand at the first time some
	 * ECMAScript code accesses it. It is freed automatically by the
	 * garbage-collecting code when the ECMAScript context is over (usually
	 * when the document is destroyed). */
	void *ecmascript_obj;
#endif
};

struct form_state {
	struct form_view *form_view;
	int g_ctrl_num;
	int position;
	enum form_type type;

	unsigned char *value;
	int state;
	int vpos;
	int vypos;
	
#ifdef CONFIG_ECMASCRIPT
	/* This holds the ECMAScript object attached to this structure. It can
	 * be NULL since the object is created on-demand at the first time some
	 * ECMAScript code accesses it. It is freed automatically by the
	 * garbage-collecting code when the ECMAScript context is over (usually
	 * when the document is destroyed). */
	void *ecmascript_obj;
#endif
};

struct submitted_value {
	LIST_HEAD(struct submitted_value);

	unsigned char *name;
	unsigned char *value;

	struct form_control *form_control;

	enum form_type type;
	int position;
};

struct submitted_value *init_submitted_value(unsigned char *name, unsigned char *value, enum form_type type, struct form_control *fc, int position);
void done_submitted_value(struct submitted_value *sv);
void done_submitted_value_list(struct list_head *list);

struct uri *get_form_uri(struct session *ses, struct document_view *doc_view, struct form_control *fc);

unsigned char *get_form_info(struct session *ses, struct document_view *doc_view);

void selected_item(struct terminal *term, void *item_, void *ses_);
int get_current_state(struct session *ses);

struct form_state *find_form_state(struct document_view *doc_view, struct form_control *fc);
struct form_control *find_form_control(struct document *document, struct form_state *fs);
struct form_view *find_form_view_in_vs(struct view_state *vs, struct form *form);
struct form_view *find_form_view(struct document_view *doc_view, struct form *form);
struct form *find_form_by_form_view(struct document *document, struct form_view *fv);

enum frame_event_status field_op(struct session *ses, struct document_view *doc_view, struct link *link, struct term_event *ev);

void draw_form_entry(struct terminal *term, struct document_view *doc_view, struct link *link);
void draw_forms(struct terminal *term, struct document_view *doc_view);

enum frame_event_status reset_form(struct session *ses, struct document_view *doc_view, int a);
enum frame_event_status submit_form(struct session *ses, struct document_view *doc_view, int do_reload);
void submit_given_form(struct session *ses, struct document_view *doc_view, struct form *form);
void auto_submit_form(struct session *ses);
void do_reset_form(struct document_view *doc_view, struct form *form);

#endif
