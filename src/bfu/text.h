/* $Id: text.h,v 1.10 2003/11/07 18:45:39 jonas Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/style.h"
#include "terminal/terminal.h"
#include "util/color.h"

void dlg_format_text(struct terminal *term,
		    unsigned char *text, int x, int *y, int w, int *rw,
		    struct color_pair *scolor, enum format_align align);

#endif
