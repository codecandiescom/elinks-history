/* $Id: text.h,v 1.2 2003/05/04 17:25:51 pasky Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/align.h"
#include "terminal/terminal.h"

void max_text_width(struct terminal *, unsigned char *, int *);
void min_text_width(struct terminal *, unsigned char *, int *);
void dlg_format_text(struct terminal *, struct terminal *, unsigned char *, int, int *, int, int *, int, enum format_align);

#endif
