/* $Id: text.h,v 1.5 2003/07/31 16:56:11 jonas Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/align.h"
#include "terminal/terminal.h"

void text_width(struct terminal *term, register unsigned char *text, int *minwidth, int *maxwidth);
void max_text_width(struct terminal *, unsigned char *, int *);
void min_text_width(struct terminal *, unsigned char *, int *);
void dlg_format_text(struct terminal *, struct terminal *, unsigned char *, int, int *, int, int *, unsigned char, enum format_align);

#endif
