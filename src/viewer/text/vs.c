/* View state manager */
/* $Id: vs.c,v 1.17 2003/10/05 14:05:08 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


void
init_vs(struct view_state *vs, unsigned char *url)
{
	int url_len = strlen(url);

	/* No need to add a byte for the \0 to url_len:
	 * struct view_state has an unsigned char url[1]. -- Miciah */
	memset(vs, 0, sizeof(struct view_state) + url_len);
	vs->current_link = -1;
	vs->plain = -1;
	memcpy(vs->url, url, url_len);
	vs->url_len = url_len;
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
}

void
copy_vs(struct view_state *dst, struct view_state *src)
{
	memcpy(dst, src, sizeof(struct view_state));

	/* XXX: Beware @dst should point to a large enough memory space
	 * to copy @src _and_ @src->url string. If it's not the case
	 * an overflow will occur inducing a totally unexpected behavior
	 * and a crash sooner or later.
	 * I warned you. --Zas */
	assert(dst->url_len >= src->url_len);
	if_assert_failed return;

	memcpy(dst->url, src->url, src->url_len + 1);

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
check_vs(struct document_view *f)
{
	struct view_state *vs = f->vs;

	if (vs->current_link >= f->document->nlinks)
		vs->current_link = f->document->nlinks - 1;

	if (vs->current_link != -1) {
		if (!c_in_view(f)) {
			set_pos_x(f, &f->document->links[vs->current_link]);
			set_pos_y(f, &f->document->links[vs->current_link]);
		}
	} else {
		find_link(f, 1, 0);
	}
}

void
next_frame(struct session *ses, int p)
{
	struct view_state *vs;
	struct document_view *fd;
	int n;

	if (!have_location(ses)
	    || (ses->screen && !document_has_frames(ses->screen->document)))
		return;

	vs = &cur_loc(ses)->vs;

	n = 0;
	foreach (fd, ses->scrn_frames) {
		if (!document_has_frames(fd->document))
			n++;
	}

	vs->current_link += p;
	if (!n) n = 1;
	while (vs->current_link < 0) vs->current_link += n;
	vs->current_link %= n;
}
