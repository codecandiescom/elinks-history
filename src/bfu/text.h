/* $Id: text.h,v 1.3 2003/06/27 20:11:15 zas Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/align.h"
#include "terminal/terminal.h"

void min_max_text_width(struct terminal *term, register unsigned char *text, int *minwidth, int *maxwidth);
void max_text_width(struct terminal *, unsigned char *, int *);
void min_text_width(struct terminal *, unsigned char *, int *);
void dlg_format_text(struct terminal *, struct terminal *, unsigned char *, int, int *, int, int *, int, enum format_align);

#endif
