/* $Id: form.h,v 1.7 2003/10/17 14:02:51 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_FORM_H
#define EL__VIEWER_TEXT_FORM_H

#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/lists.h" /* LIST_HEAD */

struct submitted_value {
	LIST_HEAD(struct submitted_value);

	unsigned char *name;
	unsigned char *value;

	struct form_control *frm;

	void *file_content;

	int fc_len;
	int type;
	int position;
};

unsigned char *get_form_url(struct session *, struct document_view *,
			    struct form_control *);

void fixup_select_state(struct form_control *fc, struct form_state *fs);
struct form_state *find_form_state(struct document_view *doc_view, struct form_control *frm);

int field_op(struct session *ses, struct document_view *doc_view, struct link *l, struct term_event *ev, int rep);

void draw_form_entry(struct terminal *t, struct document_view *doc_view, struct link *l);
void draw_forms(struct terminal *t, struct document_view *doc_view);

int has_form_submit(struct document *document, struct form_control *frm);

int submit_form(struct terminal *term, void *xxx, struct session *ses);
int submit_form_reload(struct terminal *term, void *xxx, struct session *ses);

void done_form_control(struct form_control *fc);

#endif
