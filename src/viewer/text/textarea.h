/* $Id: textarea.h,v 1.11 2003/12/01 14:08:20 pasky Exp $ */

#ifndef EL__VIEWER_TEXT_TEXTAREA_H
#define EL__VIEWER_TEXT_TEXTAREA_H

/* This file is largely a supserset of this header, so it doesn't hurt to just
 * include it here, IMHO. --pasky */
#include "viewer/text/form.h"

struct document_view;
struct link;
struct session;
struct terminal;

int area_cursor(struct form_control *frm, struct form_state *fs);
void draw_textarea(struct terminal *t, struct form_state *fs, struct document_view *doc_view, struct link *l);
unsigned char *encode_textarea(struct submitted_value *sv);

extern int textarea_editor;
void textarea_edit(int, struct terminal *, struct form_control *, struct form_state *, struct document_view *, struct link *);
int menu_textarea_edit(struct terminal *term, void *xxx, struct session *ses);

int textarea_op_home(struct form_state *fs, struct form_control *frm, int rep);
int textarea_op_up(struct form_state *fs, struct form_control *frm, int rep);
int textarea_op_down(struct form_state *fs, struct form_control *frm, int rep);
int textarea_op_end(struct form_state *fs, struct form_control *frm, int rep);
int textarea_op_enter(struct form_state *fs, struct form_control *frm, int rep);

void set_textarea(struct session *ses, struct document_view *doc_view, int kbd);

#endif
