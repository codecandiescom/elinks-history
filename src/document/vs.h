/* $Id: vs.h,v 1.2 2002/05/08 13:55:03 pasky Exp $ */

/* Placing this before the #ifndef, we'll fix some crossdep problems. */
#include "document/html/parser.h"
#include "document/html/renderer.h"

#ifndef EL__DOCUMENT_VS_H
#define EL__DOCUMENT_VS_H

struct view_state {
	int view_pos;
	int view_posx;
	int current_link;
	int plain;
	unsigned char *goto_position;
	struct form_state *form_info;
	int form_info_len;
	struct f_data_c *f;
	unsigned char url[1];
};

void init_vs(struct view_state *, unsigned char *);
void destroy_vs(struct view_state *);
void copy_vs(struct view_state *, struct view_state *);
void check_vs(struct f_data_c *);

void next_frame(struct session *, int);

#endif
