/* HTML frames parser */
/* $Id: frames.c,v 1.17 2003/10/17 13:15:57 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/frames.h"

#include "document/options.h"
#include "lowlevel/ttime.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/string.h"


static void
add_frameset_entry(struct frameset_desc *fsd, struct frameset_desc *subframe,
		   unsigned char *name, unsigned char *url)
{
	int idx;

	assert(fsd);
	if_assert_failed return;

	/* FIXME: The following is triggered by
	 * http://www.thegoodlookingorganisation.co.uk/main.htm.
	 * There may exist a true fix for this... --Zas */
	/* May the one truly fixing this notify'n'close bug 237 in the
	 * Bugzilla... --pasky */
	if (fsd->yp >= fsd->y) return;

	idx = fsd->xp + fsd->yp * fsd->x;
	fsd->f[idx].subframe = subframe;
	fsd->f[idx].name = name ? stracpy(name) : NULL;
	fsd->f[idx].url = url ? stracpy(url) : NULL;
	fsd->xp++;
	if (fsd->xp >= fsd->x) {
		fsd->xp = 0;
		fsd->yp++;
	}
}

struct frameset_desc *
create_frameset(struct document *document, struct frameset_param *fp)
{
	struct frameset_desc *fd;
	unsigned int size;

	assert(document && fp);
	if_assert_failed return NULL;

	assertm(fp->x > 0 && fp->y > 0,
		"Bad size of frameset: x=%d y=%d", fp->x, fp->y);
	if_assert_failed {
		if (fp->x <= 0) fp->x = 1;
		if (fp->y <= 0) fp->y = 1;
	}

	if (!fp->parent && document->frame_desc) return NULL;

	size = fp->x * fp->y;
	/* size - 1 since one struct frame_desc is already reserved
	 * in struct frameset_desc. */
	fd = mem_calloc(1, sizeof(struct frameset_desc)
			   + (size - 1) * sizeof(struct frame_desc));
	if (!fd) return NULL;

	{
		register int i;

		for (i = 0; i < size; i++) {
			fd->f[i].xw = fp->xw[i % fp->x];
			fd->f[i].yw = fp->yw[i / fp->x];
		}
	}

	fd->n = size;
	fd->x = fp->x;
	fd->y = fp->y;

	if (fp->parent)
		add_frameset_entry(fp->parent, fd, NULL, NULL);
	else if (!document->frame_desc)
		document->frame_desc = fd;

	return fd;
}

void
create_frame(struct frame_param *fp)
{
	assert(fp && fp->parent);
	if_assert_failed return;

	add_frameset_entry(fp->parent, NULL, fp->name, fp->url);
}

static void
add_frame_to_list(struct session *ses, struct document_view *doc_view)
{
	struct document_view *f;

	assert(ses && doc_view);
	if_assert_failed return;

	foreach (f, ses->scrn_frames) {
		if (f->yp > doc_view->yp || (f->yp == doc_view->yp && f->xp > doc_view->xp)) {
			add_at_pos(f->prev, doc_view);
			return;
		}
	}

	add_to_list_bottom(ses->scrn_frames, doc_view);
}

static struct document_view *
find_fd(struct session *ses, unsigned char *name,
	int depth, int x, int y)
{
	struct document_view *doc_view;

	assert(ses && name);
	if_assert_failed return NULL;

	foreachback (doc_view, ses->scrn_frames) {
		if (!doc_view->used && !strcasecmp(doc_view->name, name)) {
			doc_view->used = 1;
			doc_view->depth = depth;
			return doc_view;
		}
	}

	doc_view = mem_calloc(1, sizeof(struct document_view));
	if (!doc_view) return NULL;

	doc_view->used = 1;
	doc_view->name = stracpy(name);
	if (!doc_view->name) {
		mem_free(doc_view);
		return NULL;
	}
	doc_view->depth = depth;
	doc_view->xp = x;
	doc_view->yp = y;
	doc_view->search_word = &ses->search_word;

	/*add_to_list(ses->scrn_frames, doc_view);*/
	add_frame_to_list(ses, doc_view);

	return doc_view;
}

struct document_view *
format_frame(struct session *ses, unsigned char *name,
	     struct document_options *o, int depth)
{
	struct cache_entry *ce;
	struct view_state *vs;
	struct document_view *doc_view;
	struct frame *fr;

	assert(ses && name && o);
	if_assert_failed return NULL;

repeat:
	fr = ses_find_frame(ses, name);
	if (!fr) return NULL;

	vs = &fr->vs;
	if (!find_in_cache(vs->url, &ce) || !ce) return NULL;

	if (ce->redirect && fr->redirect_cnt < MAX_REDIRECTS) {
		unsigned char *u = join_urls(vs->url, ce->redirect);

		if (u) {
			fr->redirect_cnt++;
			ses_change_frame_url(ses, name, u);
			mem_free(u);
			goto repeat;
		}
	}

	doc_view = find_fd(ses, name, depth, o->xp, o->yp);
	if (doc_view) cached_format_html(vs, doc_view, o);

	return doc_view;
}

void
format_frames(struct session *ses, struct frameset_desc *fsd,
	      struct document_options *op, int depth)
{
	struct document_options o;
	register int j, n;

	assert(ses && fsd && op);
	if_assert_failed return;

	if (depth > HTML_MAX_FRAME_DEPTH) return;

	memcpy(&o, op, sizeof(struct document_options));

	o.margin = !!o.margin;

	n = 0;
	for (j = 0; j < fsd->y; j++) {
		register int i;

		o.xp = op->xp;
		for (i = 0; i < fsd->x; i++) {
			struct frame_desc *f = &fsd->f[n];

			o.xw = f->xw;
			o.yw = f->yw;
			o.framename = f->name;
			if (f->subframe)
				format_frames(ses, f->subframe, &o, depth + 1);
			else if (f->name) {
				struct document_view *doc_view;

				doc_view = format_frame(ses, f->name, &o, depth);
				if (doc_view && document_has_frames(doc_view->document))
					format_frames(ses, doc_view->document->frame_desc,
						      &o, depth + 1);
			}
			o.xp += o.xw + 1;
			n++;
		}
		o.yp += o.yw + 1;
	}
}
