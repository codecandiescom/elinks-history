/* $Id: style.h,v 1.4 2004/06/28 11:07:10 jonas Exp $ */

#ifndef EL__BFU_STYLE_H
#define EL__BFU_STYLE_H

struct color_pair;
struct terminal;

enum format_align {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT,
	ALIGN_BLOCK,
	ALIGN_NONE,
};

/* Get the colors of the bfu element. If @color is 0 a style suitable for
 * mono terminals is returned else a style for a color terminal. */
struct color_pair *
get_bfu_color(struct terminal *term, unsigned char *stylename);

void done_bfu_colors(void);

#endif
