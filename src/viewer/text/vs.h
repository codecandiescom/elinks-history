/* $Id: vs.h,v 1.4 2003/09/12 23:37:16 zas Exp $ */

/* Placing this before the #ifndef, we'll fix some crossdep problems. */
#include "document/html/parser.h"
#include "document/html/renderer.h"

#ifndef EL__VIEWER_TEXT_VS_H
#define EL__VIEWER_TEXT_VS_H

struct view_state {
	unsigned char *goto_position;
	struct document_view *view;
	struct form_state *form_info;
	int form_info_len;
	int view_pos;
	int view_posx;
	int current_link;
	int plain;
	int url_len;
	unsigned char url[1];	/* Must be last. */
};

void init_vs(struct view_state *, unsigned char *);
void destroy_vs(struct view_state *);
void copy_vs(struct view_state *, struct view_state *);
void check_vs(struct document_view *);

void next_frame(struct session *, int);

#endif
