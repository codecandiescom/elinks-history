/* HTML frames parser */
/* $Id: frames.c,v 1.31 2003/10/31 01:38:12 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/frames.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "document/view.h"
#include "lowlevel/ttime.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/string.h"


static void
add_frameset_entry(struct frameset_desc *frameset_desc,
		   struct frameset_desc *subframe,
		   unsigned char *name, unsigned char *url)
{
	struct frame_desc *frame_desc;
	int offset;

	assert(frameset_desc);
	if_assert_failed return;

	/* FIXME: The following is triggered by
	 * http://www.thegoodlookingorganisation.co.uk/main.htm.
	 * There may exist a true fix for this... --Zas */
	/* May the one truly fixing this notify'n'close bug 237 in the
	 * Bugzilla... --pasky */
	if (frameset_desc->y >= frameset_desc->height) return;

	offset = frameset_desc->x + frameset_desc->y * frameset_desc->width;
	frame_desc = &frameset_desc->frame_desc[offset];
	frame_desc->subframe = subframe;
	frame_desc->name = name ? stracpy(name) : NULL;
	frame_desc->url = url ? stracpy(url) : NULL;

	frameset_desc->x++;
	if (frameset_desc->x >= frameset_desc->width) {
		frameset_desc->x = 0;
		frameset_desc->y++;
		/* FIXME: check y here ? --Zas */
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
			fd->frame_desc[i].width = fp->width[i % fp->x];
			fd->frame_desc[i].height = fp->height[i / fp->x];
		}
	}

	fd->n = size;
	fd->width = fp->x;
	fd->height = fp->y;

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
	struct document_view *ses_doc_view;

	assert(ses && doc_view);
	if_assert_failed return;

	foreach (ses_doc_view, ses->scrn_frames) {
		if (ses_doc_view->y > doc_view->y
		    || (ses_doc_view->y == doc_view->y
			&& ses_doc_view->x > doc_view->x)) {
			add_at_pos(ses_doc_view->prev, doc_view);
			return;
		}
	}

	add_to_list_end(ses->scrn_frames, doc_view);
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
	doc_view->x = x;
	doc_view->y = y;
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
	struct frame *frame;

	assert(ses && name && o);
	if_assert_failed return NULL;

repeat:
	frame = ses_find_frame(ses, name);
	if (!frame) return NULL;

	vs = &frame->vs;
	if (!find_in_cache(vs->url, &ce) || !ce) return NULL;

	if (ce->redirect && frame->redirect_cnt < MAX_REDIRECTS) {
		unsigned char *u = join_urls(vs->url, ce->redirect);

		if (u) {
			frame->redirect_cnt++;
			ses_change_frame_url(ses, name, u);
			mem_free(u);
			goto repeat;
		}
	}

	doc_view = find_fd(ses, name, depth, o->x, o->y);
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
	for (j = 0; j < fsd->height; j++) {
		register int i;

		o.x = op->x;
		for (i = 0; i < fsd->width; i++) {
			struct frame_desc *frame_desc = &fsd->frame_desc[n];

			o.width = frame_desc->width;
			o.height = frame_desc->height;
			o.framename = frame_desc->name;
			if (frame_desc->subframe)
				format_frames(ses, frame_desc->subframe, &o, depth + 1);
			else if (frame_desc->name) {
				struct document_view *doc_view;

				doc_view = format_frame(ses, frame_desc->name, &o, depth);
				if (doc_view && document_has_frames(doc_view->document))
					format_frames(ses, doc_view->document->frame_desc,
						      &o, depth + 1);
			}
			o.x += o.width + 1;
			n++;
		}
		o.y += o.height + 1;
	}
}
