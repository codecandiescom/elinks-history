/* $Id: text.h,v 1.7 2003/08/23 16:44:42 jonas Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/style.h"
#include "terminal/terminal.h"
#include "util/color.h"

void text_width(struct terminal *term, register unsigned char *text, int *minwidth, int *maxwidth);
void max_text_width(struct terminal *, unsigned char *, int *);
void min_text_width(struct terminal *, unsigned char *, int *);

void dlg_format_text(struct terminal *term, struct terminal *t2,
		    unsigned char *text, int x, int *y, int w, int *rw,
		    struct color_pair *scolor, enum format_align align);

#endif
