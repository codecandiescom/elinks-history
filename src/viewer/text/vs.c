/* View state manager */
/* $Id: vs.c,v 1.49 2004/10/10 20:36:08 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


void
init_vs(struct view_state *vs, struct uri *uri, int plain)
{
	memset(vs, 0, sizeof(struct view_state));
	vs->current_link = -1;
	vs->plain = plain;
	vs->uri = uri ? get_uri_reference(uri) : NULL;
	vs->did_fragment = !uri->fragmentlen;
#ifdef CONFIG_ECMASCRIPT
	/* If we ever get to render this vs, give it an interpreter. */
	vs->ecmascript_fragile = 1;
#endif
}

void
destroy_vs(struct view_state *vs, int blast_ecmascript)
{
	int i;

	for (i = 0; i < vs->form_info_len; i++)
		mem_free_if(vs->form_info[i].value);

	if (vs->uri) done_uri(vs->uri);
	mem_free_if(vs->form_info);
#ifdef CONFIG_ECMASCRIPT
	if (blast_ecmascript && vs->ecmascript)
		ecmascript_put_interpreter(vs->ecmascript);
#endif
	if (vs->doc_view) {
		vs->doc_view->vs = NULL;
		vs->doc_view = NULL;
	}
}

void
copy_vs(struct view_state *dst, struct view_state *src)
{
	memcpy(dst, src, sizeof(struct view_state));

	/* We do not copy ecmascript stuff around since it's specific for
	 * a single location, offsprings (followups and so) nedd their own. */
#ifdef CONFIG_ECMASCRIPT
	dst->ecmascript = NULL;
	/* If we ever get to render this vs, give it an interpreter. */
	dst->ecmascript_fragile = 1;
#endif

	/* Clean as a baby. */
	dst->doc_view = NULL;

	dst->uri = get_uri_reference(src->uri);
	/* Redo fragment if there is one? */
	dst->did_fragment = !src->uri->fragmentlen;

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
		find_link_page_down(doc_view);
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

	ses->navigate_mode = NAVIGATE_LINKWISE;

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
