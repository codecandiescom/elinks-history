/* $Id: style.h,v 1.1 2003/08/23 04:33:47 jonas Exp $ */

#ifndef EL__BFU_STYLE_H
#define EL__BFU_STYLE_H

#include "terminal/terminal.h"

enum format_align {
	AL_LEFT,
	AL_CENTER,
	AL_RIGHT,
	AL_BLOCK,
	AL_NONE,
};

/* Get the colors of the bfu element. If @color is 0 a style suitable for
 * mono terminals is returned else a style for a color terminal. */
struct screen_color *
get_bfu_color(struct terminal *term, unsigned char *stylename);

void done_bfu_colors(void);

#endif
