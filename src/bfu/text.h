/* $Id: text.h,v 1.1 2002/07/04 15:45:38 pasky Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/align.h"
#include "lowlevel/terminal.h"

void max_text_width(struct terminal *, unsigned char *, int *);
void min_text_width(struct terminal *, unsigned char *, int *);
void dlg_format_text(struct terminal *, struct terminal *, unsigned char *, int, int *, int, int *, int, enum format_align);

#endif
