/* Sessions task managment */
/* $Id: task.c,v 1.1 2003/12/06 02:56:02 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/view.h"
#include "protocol/protocol.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "sched/session.h"
#include "sched/task.h"
#include "viewer/text/view.h"

static void
do_follow_url(struct session *ses, unsigned char *url, unsigned char *target,
	      enum task_type task, enum cache_mode cache_mode, int do_referrer)
{
	unsigned char *u, *referrer = NULL;
	unsigned char *pos;
	protocol_external_handler *fn;
	enum protocol protocol = known_protocol(url, NULL);

	if (protocol == PROTOCOL_UNKNOWN) {
		print_unknown_protocol_dialog(ses);
		return;
	}

	if (protocol != PROTOCOL_INVALID) {
		fn = get_protocol_external_handler(protocol);
		if (fn) {
			fn(ses, url);
			return;
		}
	}

	ses->reloadlevel = cache_mode;

	u = translate_url(url, ses->tab->term->cwd);
	if (!u) {
		struct download stat = { NULL_LIST_HEAD, NULL, NULL,
					 NULL, NULL, NULL,
					 S_BAD_URL, PRI_CANCEL, 0 };

		print_error_dialog(ses, &stat);
		return;
	}

	pos = extract_fragment(u);

	if (ses->task == task) {
		if (!strcmp(ses->loading_url, u)) {
			/* We're already loading the URL. */
			mem_free(u);

			if (ses->goto_position)
				mem_free(ses->goto_position);
			ses->goto_position = pos;

			return;
		}
	}

	abort_loading(ses, 0);

	if (do_referrer) {
		struct document_view *doc_view = current_frame(ses);

		if (doc_view && doc_view->document)
			referrer = doc_view->document->url;
	}

	set_referrer(ses, referrer);

	ses_goto(ses, u, target, NULL,
		 PRI_MAIN, cache_mode, task,
		 pos, end_load, 0);

	/* abort_loading(ses); */
}

static void
follow_url(struct session *ses, unsigned char *url, unsigned char *target,
	   enum task_type task, enum cache_mode cache_mode, int referrer)
{
	unsigned char *new_url = url;
#ifdef HAVE_SCRIPTING
	static int follow_url_event_id = EVENT_NONE;

	set_event_id(follow_url_event_id, "follow-url");
	trigger_event(follow_url_event_id, &new_url, ses);
	if (!new_url) return;
#endif

	if (*new_url)
		do_follow_url(ses, new_url, target, task, cache_mode, referrer);

#ifdef HAVE_SCRIPTING
	if (new_url != url) mem_free(new_url);
#endif
}

void
goto_url_frame_reload(struct session *ses, unsigned char *url,
		      unsigned char *target)
{
	follow_url(ses, url, target, TASK_FORWARD, CACHE_MODE_FORCE_RELOAD, 1);
}

void
goto_url_frame(struct session *ses, unsigned char *url,
	       unsigned char *target)
{
	follow_url(ses, url, target, TASK_FORWARD, CACHE_MODE_NORMAL, 1);
}

void
map_selected(struct terminal *term, struct link_def *ld, struct session *ses)
{
	goto_url_frame(ses, ld->link, ld->target);
}

void
goto_url(struct session *ses, unsigned char *url)
{
	follow_url(ses, url, NULL, TASK_FORWARD, CACHE_MODE_NORMAL, 0);
}

void
goto_url_with_hook(struct session *ses, unsigned char *url)
{
	unsigned char *new_url = url;
#ifdef HAVE_SCRIPTING
	static int goto_url_event_id = EVENT_NONE;

	set_event_id(goto_url_event_id, "goto-url");
	trigger_event(goto_url_event_id, &new_url, ses);
	if (!new_url) return;
#endif

	if (*new_url) goto_url(ses, new_url);

#ifdef HAVE_SCRIPTING
	if (new_url != url) mem_free(new_url);
#endif
}

/* TODO: Should there be goto_imgmap_reload() ? */

void
goto_imgmap(struct session *ses, unsigned char *url, unsigned char *href,
	    unsigned char *target)
{
	if (ses->imgmap_href_base) mem_free(ses->imgmap_href_base);
	ses->imgmap_href_base = href;
	if (ses->imgmap_target_base) mem_free(ses->imgmap_target_base);
	ses->imgmap_target_base = target;
	follow_url(ses, url, target, TASK_IMGMAP, CACHE_MODE_NORMAL, 1);
}
