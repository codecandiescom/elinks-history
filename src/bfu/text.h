/* $Id: text.h,v 1.23 2004/11/19 11:12:53 zas Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/style.h"
#include "bfu/widget.h"
#include "util/color.h"

struct dialog;
struct terminal;

void add_dlg_text(struct dialog *dlg, unsigned char *text,
		  enum format_align align, int bottom_pad);

extern struct widget_ops text_ops;
void dlg_format_text_do(struct terminal *term,
		    unsigned char *text, int x, int *y, int w, int *rw,
		    struct color_pair *scolor, enum format_align align);

void
dlg_format_text(struct terminal *term, struct widget_data *widget_data,
		int x, int *y, int dlg_width, int *real_width, int height);

#endif
