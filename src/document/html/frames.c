/* HTML frames parser */
/* $Id: frames.c,v 1.6 2003/07/15 20:18:08 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/frames.h"

#include "document/html/colors.h"
#include "document/options.h"
#include "lowlevel/ttime.h"
#include "protocol/uri.h"
#include "terminal/draw.h" /* chr type */
#include "util/string.h"



static void
add_frameset_entry(struct frameset_desc *fsd,
		   struct frameset_desc *subframe,
		   unsigned char *name, unsigned char *url)
{
	int idx;

	assert(fsd && fsd->yp < fsd->y);
	if_assert_failed return;

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
create_frameset(struct document *fda, struct frameset_param *fp)
{
	struct frameset_desc *fd;

	assert(fp);
	if_assert_failed return NULL;
	assertm(fp->x > 0 && fp->y > 0,
		"Bad size of frameset: x=%d y=%d", fp->x, fp->y);
	if_assert_failed {
		if (fp->x <= 0) fp->x = 1;
		if (fp->y <= 0) fp->y = 1;
	}

	fd = mem_calloc(1, sizeof(struct frameset_desc)
			   + fp->x * fp->y * sizeof(struct frame_desc));
	if (!fd) return NULL;

	fd->n = fp->x * fp->y;
	fd->x = fp->x;
	fd->y = fp->y;

	{
		register int i;

		for (i = 0; i < fd->n; i++) {
			fd->f[i].xw = fp->xw[i % fp->x];
			fd->f[i].yw = fp->yw[i / fp->x];
		}
	}

	if (fp->parent) add_frameset_entry(fp->parent, fd, NULL, NULL);
	else if (!fda->frame_desc) fda->frame_desc = fd;
	     else mem_free(fd), fd = NULL;

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
add_frame_to_list(struct session *ses, struct document_view *fd)
{
	struct document_view *f;

	assert(ses && fd);
	if_assert_failed return;

	foreach (f, ses->scrn_frames) {
		if (f->yp > fd->yp || (f->yp == fd->yp && f->xp > fd->xp)) {
			add_at_pos(f->prev, fd);
			return;
		}
	}

	add_to_list_bottom(ses->scrn_frames, fd);
}

static struct document_view *
find_fd(struct session *ses, unsigned char *name,
	int depth, int x, int y)
{
	struct document_view *fd;

	assert(ses && name);
	if_assert_failed return NULL;

	foreachback (fd, ses->scrn_frames) {
		if (!fd->used && !strcasecmp(fd->name, name)) {
			fd->used = 1;
			fd->depth = depth;
			return fd;
		}
	}

	fd = mem_calloc(1, sizeof(struct document_view));
	if (!fd) return NULL;

	fd->used = 1;
	fd->name = stracpy(name);
	if (!fd->name) {
		mem_free(fd);
		return NULL;
	}
	fd->depth = depth;
	fd->xp = x;
	fd->yp = y;
	fd->search_word = &ses->search_word;

	/*add_to_list(ses->scrn_frames, fd);*/
	add_frame_to_list(ses, fd);

	return fd;
}

struct document_view *
format_frame(struct session *ses, unsigned char *name,
	     struct document_options *o, int depth)
{
	struct cache_entry *ce;
	struct view_state *vs;
	struct document_view *fd;
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

	fd = find_fd(ses, name, depth, o->xp, o->yp);
	if (fd) cached_format_html(vs, fd, o);

	return fd;
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

	if (o.margin) o.margin = 1;

	n = 0;
	for (j = 0; j < fsd->y; j++) {
		register int i;

		o.xp = op->xp;
		for (i = 0; i < fsd->x; i++) {
			struct document_view *fdc;

			o.xw = fsd->f[n].xw;
			o.yw = fsd->f[n].yw;
			o.framename = fsd->f[n].name;
			if (fsd->f[n].subframe)
				format_frames(ses, fsd->f[n].subframe, &o, depth + 1);
			else if (fsd->f[n].name) {
				fdc = format_frame(ses, fsd->f[n].name, &o, depth);
				if (fdc && fdc->document && fdc->document->frame)
					format_frames(ses, fdc->document->frame_desc,
						      &o, depth + 1);
			}
			o.xp += o.xw + 1;
			n++;
		}
		o.yp += o.yw + 1;
	}
}
