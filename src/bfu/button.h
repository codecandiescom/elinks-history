/* $Id: button.h,v 1.3 2002/07/04 15:45:38 pasky Exp $ */

#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "bfu/align.h"
#include "bfu/bfu.h"
#include "lowlevel/terminal.h"

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

void max_buttons_width(struct terminal *, struct widget_data *, int, int *);
void min_buttons_width(struct terminal *, struct widget_data *, int, int *);
void dlg_format_buttons(struct terminal *, struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align);

#endif
