/* $Id: style.h,v 1.3 2003/12/01 14:36:52 pasky Exp $ */

#ifndef EL__BFU_STYLE_H
#define EL__BFU_STYLE_H

struct color_pair;
struct terminal;

enum format_align {
	AL_LEFT,
	AL_CENTER,
	AL_RIGHT,
	AL_BLOCK,
	AL_NONE,
};

/* Get the colors of the bfu element. If @color is 0 a style suitable for
 * mono terminals is returned else a style for a color terminal. */
struct color_pair *
get_bfu_color(struct terminal *term, unsigned char *stylename);

void done_bfu_colors(void);

#endif
