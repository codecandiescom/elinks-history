/* $Id: vs.h,v 1.14 2004/03/22 03:01:19 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_VS_H
#define EL__VIEWER_TEXT_VS_H

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
	int wrap:1;

	int url_len;
	unsigned char url[1];	/* Must be last. */
};

#define get_vs_url_copy(vs)	memacpy((vs)->url, (vs)->url_len)
#define get_vs_cache_entry(vs)	find_in_cache((vs)->url)

void init_vs(struct view_state *, unsigned char *, int);
void destroy_vs(struct view_state *);

void copy_vs(struct view_state *, struct view_state *);
void check_vs(struct document_view *);

void next_frame(struct session *, int);

#endif
