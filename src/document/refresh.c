/* Document (meta) refresh. */
/* $Id: refresh.c,v 1.18 2004/03/31 22:00:51 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "document/document.h"
#include "document/refresh.h"
#include "document/view.h"
#include "lowlevel/select.h"
#include "sched/session.h"
#include "sched/task.h"
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

	/* When refreshing documents that will trigger a download (like
	 * sourceforge's download pages) make sure that we do not endlessly
	 * trigger the download (bug 289). */
	foreach (type_query, ses->tq)
		if (!strcasecmp(refresh->url, type_query->url))
			return;

	if (!strcasecmp(refresh->url, ses->doc_view->document->uri)) {
		/* If the refreshing is for the current URI, force a reload. */
		reload(ses, CACHE_MODE_FORCE_RELOAD);
	} else {
		/* This makes sure that we send referer. */
		goto_url_frame(ses, refresh->url, NULL);
	}
}

void
start_document_refresh(struct document_refresh *refresh, struct session *ses)
{
	int minimum = get_opt_int("document.browse.minimum_refresh_time");
	int time = int_max(1000 * refresh->seconds, minimum);

	refresh->timer = install_timer(time, do_document_refresh, ses);
}
