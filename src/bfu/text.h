/* $Id: text.h,v 1.6 2003/08/23 03:31:41 jonas Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/style.h"
#include "terminal/terminal.h"
#include "terminal/draw.h"

void text_width(struct terminal *term, register unsigned char *text, int *minwidth, int *maxwidth);
void max_text_width(struct terminal *, unsigned char *, int *);
void min_text_width(struct terminal *, unsigned char *, int *);

void dlg_format_text(struct terminal *term, struct terminal *t2,
		    unsigned char *text, int x, int *y, int w, int *rw,
		    struct screen_color *scolor, enum format_align align);

#endif
