/* The document base functionality */
/* $Id: document.c,v 1.3 2003/10/29 17:51:06 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/cache.h"
#include "document/document.h"
#include "document/html/frames.h"
#include "document/options.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"


struct document *
init_document(unsigned char *uristring, struct document_options *options)
{
	struct document *document = mem_calloc(1, sizeof(struct document));

	if (!document) return NULL;

	document->url = stracpy(uristring);
	if (!document->url) {
		mem_free(document);
		return NULL;
	}

	init_list(document->forms);
	init_list(document->tags);
	init_list(document->nodes);

	document->refcount = 1;

	copy_opt(&document->opt, options);

	return document;
}

static void
free_frameset_desc(struct frameset_desc *fd)
{
	int i;

	for (i = 0; i < fd->n; i++) {
		if (fd->f[i].subframe) free_frameset_desc(fd->f[i].subframe);
		if (fd->f[i].name) mem_free(fd->f[i].name);
		if (fd->f[i].url) mem_free(fd->f[i].url);
	}

	mem_free(fd);
}

void
done_document(struct document *document)
{
	struct cache_entry *ce;
	struct form_control *fc;
	int pos;

	assert(document);
	if_assert_failed return;

	assertm(!document->refcount, "Attempt to free locked formatted data.");
	if_assert_failed return;

	if (!find_in_cache(document->url, &ce) || !ce)
		internal("no cache entry for document");
	else
		ce->refcount--;

	if (document->url) mem_free(document->url);
	if (document->title) mem_free(document->title);
	if (document->frame_desc) free_frameset_desc(document->frame_desc);
	if (document->refresh) done_document_refresh(document->refresh);

	for (pos = 0; pos < document->nlinks; pos++) {
		done_link_members(&document->links[pos]);
	}

	if (document->links) mem_free(document->links);

	if (document->data) {
		for (pos = 0; pos < document->height; pos++) {
			if (document->data[pos].d)
				mem_free(document->data[pos].d);
		}

		mem_free(document->data);
	}

	if (document->lines1) mem_free(document->lines1);
	if (document->lines2) mem_free(document->lines2);
	if (document->opt.framename) mem_free(document->opt.framename);

	foreach (fc, document->forms) {
		done_form_control(fc);
	}

	free_list(document->forms);
	free_list(document->tags);
	free_list(document->nodes);

	if (document->search) mem_free(document->search);
	if (document->slines1) mem_free(document->slines1);
	if (document->slines2) mem_free(document->slines2);

	del_from_list(document);
	mem_free(document);
}
