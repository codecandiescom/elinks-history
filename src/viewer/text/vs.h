/* $Id: vs.h,v 1.10 2003/11/24 22:58:32 pasky Exp $ */

#ifndef EL__VIEWER_TEXT_VS_H
#define EL__VIEWER_TEXT_VS_H

#include "util/object.h"

/* Crossdeps are evil. */
struct document_view;
struct session;
struct form_state;

struct view_state {
	unsigned char *goto_position;

	struct form_state *form_info;
	int form_info_len;

	int x, y;
	int current_link;

	int plain;

	int refcount; /* No direct access, use provided macros. */

	int url_len;
	unsigned char url[1];	/* Must be last. */
};

void init_vs(struct view_state *, unsigned char *, int);
void destroy_vs(struct view_state *);
#define release_vs(vs) \
do { \
	object_unlock(vs); \
	if (!is_object_used(vs) destroy_vs(vs); \
} while (0)

void copy_vs(struct view_state *, struct view_state *);
void check_vs(struct document_view *);

void next_frame(struct session *, int);

#endif
