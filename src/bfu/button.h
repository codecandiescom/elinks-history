/* $Id: button.h,v 1.8 2003/06/27 20:39:32 zas Exp $ */

#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "bfu/align.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

extern struct widget_ops button_ops;
void buttons_width(struct terminal *term, struct widget_data *butt, int n, int *minwidth, int *maxwidth);
void max_buttons_width(struct terminal *, struct widget_data *, int, int *);
void min_buttons_width(struct terminal *, struct widget_data *, int, int *);
void dlg_format_buttons(struct terminal *, struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align);

#endif
