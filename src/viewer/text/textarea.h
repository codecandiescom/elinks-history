/* $Id: textarea.h,v 1.3 2003/07/03 00:18:36 zas Exp $ */

#ifndef EL__VIEWER_TEXT_TEXTAREA_H
#define EL__VIEWER_TEXT_TEXTAREA_H

#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "terminal/terminal.h"
#include "viewer/text/view.h"

int area_cursor(struct form_control *frm, struct form_state *fs);
void draw_textarea(struct terminal *t, struct form_state *fs, struct f_data_c *f, struct link *l);
unsigned char *encode_textarea(struct submitted_value *sv);

extern int textarea_editor;
void textarea_edit(int, struct terminal *, struct form_control *, struct form_state *, struct f_data_c *, struct link *);

int textarea_op_home(struct form_state *fs, struct form_control *frm, int rep);
int textarea_op_up(struct form_state *fs, struct form_control *frm, int rep);
int textarea_op_down(struct form_state *fs, struct form_control *frm, int rep);
int textarea_op_end(struct form_state *fs, struct form_control *frm, int rep);
int textarea_op_enter(struct form_state *fs, struct form_control *frm, int rep);

void set_textarea(struct session *ses, struct f_data_c *f, int kbd);

#endif
