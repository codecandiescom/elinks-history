/* Document (meta) refresh. */
/* $Id: refresh.c,v 1.6 2003/11/23 13:59:29 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/refresh.h"
#include "lowlevel/select.h"
#include "sched/session.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


struct document_refresh *
init_document_refresh(unsigned char *url, unsigned long seconds)
{
	struct document_refresh *refresh;
	int url_len = strlen(url) + 1;

	refresh = mem_alloc(sizeof(struct document_refresh) + url_len);
	if (refresh) {
		memcpy(refresh->url, url, url_len);
		refresh->seconds = seconds;
		refresh->timer = -1;
	}

	return refresh;
};

void
kill_document_refresh(struct document_refresh *refresh)
{
	if (refresh->timer != -1) {
		kill_timer(refresh->timer);
		refresh->timer = -1;
	}
};

void
done_document_refresh(struct document_refresh *refresh)
{
	kill_document_refresh(refresh);
	mem_free(refresh);
}

static void
do_document_refresh(void *data)
{
	struct session *ses = data;
	struct document_refresh *refresh = ses->doc_view->document->refresh;
	struct tq *type_query;

	assert(refresh);

	refresh->timer = -1;

	foreach (type_query, ses->tq)
		if (!strcasecmp(refresh->url, type_query->url))
			return;

	/* This makes sure that we send referer. */
	goto_url_frame(ses, refresh->url, NULL);
};

void
start_document_refresh(struct document_refresh *refresh, struct session *ses)
{
	int time = 1000 * refresh->seconds;

	refresh->timer = install_timer(time, do_document_refresh, ses);
};
