/* View state manager */
/* $Id: vs.c,v 1.28 2004/03/22 03:47:13 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/document.h"
#include "document/view.h"
#include "sched/session.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


struct view_state *
init_vs(struct view_state *vs, unsigned char *url, int plain)
{
	int url_len = strlen(url);

	memset(vs, 0, sizeof(struct view_state));

	vs->uri = get_uri(url);
	if (!vs->uri) return NULL;

	vs->current_link = -1;
	vs->plain = plain;
	vs->url_len = url_len;

	return vs;
}

void
destroy_vs(struct view_state *vs)
{
	int i;

	if (vs->goto_position)
		mem_free(vs->goto_position);

	for (i = 0; i < vs->form_info_len; i++)
		if (vs->form_info[i].value)
			mem_free(vs->form_info[i].value);

	if (vs->form_info) mem_free(vs->form_info);
	if (vs->uri) done_uri(vs->uri);
}

void
copy_vs(struct view_state *dst, struct view_state *src)
{
	if (dst->uri) done_uri(dst->uri);
 
	memcpy(dst, src, sizeof(struct view_state));
	object_lock(dst->uri);

	dst->goto_position = src->goto_position ?
			     stracpy(src->goto_position) : NULL;

	if (src->form_info_len) {
		dst->form_info = mem_alloc(src->form_info_len
					   * sizeof(struct form_state));
		if (dst->form_info) {
			int i;

			memcpy(dst->form_info, src->form_info,
			       src->form_info_len * sizeof(struct form_state));
			for (i = 0; i < src->form_info_len; i++)
				if (src->form_info[i].value)
					dst->form_info[i].value =
						stracpy(src->form_info[i].value);
		}
	}
}

void
check_vs(struct document_view *doc_view)
{
	struct view_state *vs = doc_view->vs;

	int_upper_bound(&vs->current_link, doc_view->document->nlinks - 1);

	if (vs->current_link != -1) {
		if (!current_link_is_visible(doc_view)) {
			struct link *links = doc_view->document->links;

			set_pos_x(doc_view, &links[vs->current_link]);
			set_pos_y(doc_view, &links[vs->current_link]);
		}
	} else {
		find_link(doc_view, 1, 0);
	}
}

void
next_frame(struct session *ses, int p)
{
	struct view_state *vs;
	struct document_view *doc_view;
	int n;

	if (!have_location(ses)
	    || (ses->doc_view && !document_has_frames(ses->doc_view->document)))
		return;

	vs = &cur_loc(ses)->vs;

	n = 0;
	foreach (doc_view, ses->scrn_frames) {
		if (!document_has_frames(doc_view->document))
			n++;
	}

	vs->current_link += p;
	if (!n) n = 1;
	while (vs->current_link < 0) vs->current_link += n;
	vs->current_link %= n;
}
