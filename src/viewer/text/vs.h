/* $Id: vs.h,v 1.2 2003/07/15 20:18:11 jonas Exp $ */

/* Placing this before the #ifndef, we'll fix some crossdep problems. */
#include "document/html/parser.h"
#include "document/html/renderer.h"

#ifndef EL__VIEWER_TEXT_VS_H
#define EL__VIEWER_TEXT_VS_H

struct view_state {
	int view_pos;
	int view_posx;
	int current_link;
	int plain;
	unsigned char *goto_position;
	struct form_state *form_info;
	int form_info_len;
	struct document_view *view;
	unsigned char url[1];
};

void init_vs(struct view_state *, unsigned char *);
void destroy_vs(struct view_state *);
void copy_vs(struct view_state *, struct view_state *);
void check_vs(struct document_view *);

void next_frame(struct session *, int);

#endif
