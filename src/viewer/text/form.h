/* $Id: form.h,v 1.1 2003/07/03 00:47:29 pasky Exp $ */

#ifndef EL__VIEWER_TEXT_FORM_H
#define EL__VIEWER_TEXT_FORM_H

#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "terminal/terminal.h"
#include "sched/session.h"

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

unsigned char *get_form_url(struct session *, struct f_data_c *,
			    struct form_control *);

void fixup_select_state(struct form_control *fc, struct form_state *fs);
struct form_state *find_form_state(struct f_data_c *f, struct form_control *frm);

int field_op(struct session *ses, struct f_data_c *f, struct link *l, struct event *ev, int rep);

void draw_form_entry(struct terminal *t, struct f_data_c *f, struct link *l);
void draw_forms(struct terminal *t, struct f_data_c *f);

int has_form_submit(struct f_data *f, struct form_control *frm);

int submit_form(struct terminal *term, void *xxx, struct session *ses);
int submit_form_reload(struct terminal *term, void *xxx, struct session *ses);

#endif
