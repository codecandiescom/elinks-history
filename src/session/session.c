/* Sessions managment - you'll find things here which you wouldn't expect */
/* $Id: session.c,v 1.289 2004/01/04 15:40:54 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/leds.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "bookmarks/bookmarks.h"
#include "cache/cache.h"
#include "config/options.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/html/renderer.h"
#include "document/refresh.h"
#include "document/view.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "lowlevel/select.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/download.h"
#include "sched/error.h"
#include "sched/event.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/session.h"
#include "sched/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"
#include "util/ttime.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"

/* Unsafe macros */
#include "document/html/parser.h"


struct file_to_load {
	LIST_HEAD(struct file_to_load);

	struct session *ses;
	int req_sent;
	int pri;
	struct cache_entry *ce;
	unsigned char *target_frame;
	unsigned char *url;
	struct download stat;
};


INIT_LIST_HEAD(sessions);

static int session_id = 1;


struct file_to_load * request_additional_file(struct session *,
					      unsigned char *, unsigned char *, int);
#if 0
struct file_to_load *request_additional_loading_file(struct session *,
						     unsigned char *,
						     struct download *, int);
#endif


struct download *
get_current_download(struct session *ses)
{
	struct download *stat = NULL;

	if (!ses) return NULL;

	if (ses->task.type)
		stat = &ses->loading;
	else if (have_location(ses))
		stat = &cur_loc(ses)->download;

	if (stat && stat->state == S_OK) {
		struct file_to_load *ftl;

		foreach (ftl, ses->more_files)
			if (ftl->req_sent && ftl->stat.state >= 0)
				return &ftl->stat;
	}

	/* Note that @stat isn't necessarily NULL here,
	 * if @ses->more_files is empty. -- Miciah */
	return stat;
}

void
print_error_dialog(struct session *ses, struct download *stat)
{
	unsigned char *t = get_err_msg(stat->state, ses->tab->term);

	if (!t) return;
	msg_box(ses->tab->term, NULL, MSGBOX_NO_INTL,
		_("Error", ses->tab->term), AL_CENTER,
		t,
		ses, 1,
		_("OK", ses->tab->term), NULL, B_ENTER | B_ESC /*,
		N_("Retry", ses->tab->term), NULL, 0 */ /* !!! TODO: retry */);
}

static void
abort_files_load(struct session *ses, int interrupt)
{
	struct file_to_load *ftl;
	int q;

	do {
		q = 0;
		foreach (ftl, ses->more_files) {
			if (ftl->stat.state >= 0 && ftl->req_sent) {
				q = 1;
				change_connection(&ftl->stat, NULL, PRI_CANCEL, interrupt);
			}
		}
	} while (q);
}

void
free_files(struct session *ses)
{
	struct file_to_load *ftl;

	abort_files_load(ses, 0);
	foreach (ftl, ses->more_files) {
		if (ftl->ce) object_unlock(ftl->ce);
		if (ftl->url) mem_free(ftl->url);
		if (ftl->target_frame) mem_free(ftl->target_frame);
	}
	free_list(ses->more_files);
}




static void request_frameset(struct session *, struct frameset_desc *);

static void
request_frame(struct session *ses, unsigned char *name, unsigned char *uurl)
{
	struct location *loc = cur_loc(ses);
	struct frame *frame;
	unsigned char *url, *pos;

	assertm(have_location(ses), "request_frame: no location");
	if_assert_failed return;

	foreach (frame, loc->frames) {
		struct document_view *doc_view;

		if (strcasecmp(frame->name, name))
			continue;

		foreach(doc_view, ses->scrn_frames) {
			if (doc_view->vs == &frame->vs && doc_view->document->frame_desc) {
				request_frameset(ses, doc_view->document->frame_desc);
				return;
			}
		}

		url = memacpy(frame->vs.url, frame->vs.url_len);
		if (!url) return;
#if 0
		/* This seems not to be needed anymore, it looks like this
		 * condition should never happen. It's apparently what Mikulas
		 * thought, at least. I'll review this more carefully when I
		 * will understand this stuff better ;-). --pasky */
		if (frame->vs.view && frame->vs.view->document
		    && frame->vs.view->document->frame_desc)) {
			request_frameset(ses, frame->vs.view->document->frame_desc);
			if (url) mem_free(url);
			return;
		}
#endif
		goto found;
	}

	url = stracpy(uurl);
	if (!url) return;
	pos = extract_fragment(url);

	/* strlen(url) without + 1 since vs have already reserved one byte. */
	frame = mem_alloc(sizeof(struct frame) + strlen(url));
	if (!frame) {
		mem_free(url);
		if (pos) mem_free(pos);
		return;
	}
	memset(frame, 0, sizeof(struct frame));

	frame->name = stracpy(name);
	if (!frame->name) {
		mem_free(frame);
		mem_free(url);
		if (pos) mem_free(pos);
		return;
	}

	init_vs(&frame->vs, url, -1);
	if (pos) frame->vs.goto_position = pos;

	add_to_list(loc->frames, frame);

found:
	if (*url)
		request_additional_file(ses, name, url, PRI_FRAME);
	mem_free(url);
}

static void
request_frameset(struct session *ses, struct frameset_desc *frameset_desc)
{
	static int depth = 0; /* Inheritation counter (recursion brake ;) */

	if (++depth <= HTML_MAX_FRAME_DEPTH) {
		int i = 0;

		for (; i < frameset_desc->n; i++) {
			struct frame_desc *frame_desc = &frameset_desc->frame_desc[i];

			if (frame_desc->subframe) {
				request_frameset(ses, frame_desc->subframe);
			} else if (frame_desc->name) {
				request_frame(ses, frame_desc->name,
					      frame_desc->url);
			}
		}
	}

	depth--;
}

inline void
load_frames(struct session *ses, struct document_view *doc_view)
{
	struct document *document = doc_view->document;

	if (!document || !document->frame_desc) return;
	request_frameset(ses, document->frame_desc);
}

void
display_timer(struct session *ses)
{
	ttime t = get_time();

	draw_formatted(ses, 1);

	t = (get_time() - t) * DISPLAY_TIME;
	if (t < DISPLAY_TIME_MIN) t = DISPLAY_TIME_MIN;
	ses->display_timer = install_timer(t, (void (*)(void *))display_timer,
					   ses);

	load_frames(ses, ses->doc_view);
	process_file_requests(ses);
}


struct questions_entry {
	LIST_HEAD(struct questions_entry);

	void (*callback)(struct session *);
};

INIT_LIST_HEAD(questions_queue);


void
check_questions_queue(struct session *ses)
{
	struct questions_entry *q;

	while (!list_empty(questions_queue)) {
		q = questions_queue.next;
		q->callback(ses);
		del_from_list(q);
		mem_free(q);
	}
}

void
add_questions_entry(void *callback)
{
	struct questions_entry *q = mem_alloc(sizeof(struct questions_entry));

	if (!q) return;
	q->callback = callback;
	add_to_list(questions_queue, q);
}

#ifdef HAVE_SCRIPTING
static void
maybe_pre_format_html(struct cache_entry *ce, struct session *ses)
{
	struct fragment *fr;
	unsigned char *src;
	int len;
	static int pre_format_html_event = EVENT_NONE;

	if (!ce || ce->done_pre_format_html_hook || list_empty(ce->frag))
		return;

	defrag_entry(ce);
	fr = ce->frag.next;
	src = fr->data;
	len = fr->length;

	set_event_id(pre_format_html_event, "pre-format-html");
	trigger_event(pre_format_html_event, &src, &len, ses, get_cache_uri(ce));

	if (src && src != fr->data) {
		add_fragment(ce, 0, src, len);
		truncate_entry(ce, len, 1);
		ce->incomplete = 0; /* XXX */
		mem_free(src);
	}

	ce->done_pre_format_html_hook = 1;
}
#endif

void
doc_end_load(struct download *stat, struct session *ses)
{
	int submit = 0;
	struct form_control *fc = NULL;

	if (stat->state < 0) {
#ifdef HAVE_SCRIPTING
		maybe_pre_format_html(stat->ce, ses);
#endif
		if (ses->display_timer != -1) {
			kill_timer(ses->display_timer);
			ses->display_timer = -1;
		}

		draw_formatted(ses, 1);

		if (get_opt_bool_tree(cmdline_options, "auto-submit")) {
			if (!list_empty(ses->doc_view->document->forms)) {
				get_opt_bool_tree(cmdline_options,
						  "auto-submit") = 0;
				submit = 1;
			}
		}

		load_frames(ses, ses->doc_view);
		process_file_requests(ses);

		if (ses->doc_view->document->refresh
		    && get_opt_bool("document.browse.refresh")) {
			start_document_refresh(ses->doc_view->document->refresh,
					       ses);
		}

		if (stat->state != S_OK) {
			print_error_dialog(ses, stat);
		}

	} else if (ses->display_timer == -1) {
		display_timer(ses);
	}

	check_questions_queue(ses);
	print_screen_status(ses);

#ifdef CONFIG_GLOBHIST
	if (stat->conn) {
		unsigned char *title = ses->doc_view->document->title;
		struct uri *uri = &stat->conn->uri;
		unsigned char *uristring = uri->protocol == PROTOCOL_PROXY
					 ? uri->data : struri(*uri);

		add_global_history_item(uristring, title, time(NULL));
	}
#endif

	if (submit) {
		goto_link(get_form_url(ses, ses->doc_view, fc), fc->target, ses,
			  1);
	}
}

void
file_end_load(struct download *stat, struct file_to_load *ftl)
{
	if (ftl->stat.ce) {
		if (ftl->ce) object_unlock(ftl->ce);
		ftl->ce = ftl->stat.ce;
		object_lock(ftl->ce);
	}

	/* FIXME: We need to do content-type check here! However, we won't
	 * handle properly the "Choose action" dialog now :(. */
	if (ftl->ce && !ftl->ce->redirect_get) {
		struct session *ses = ftl->ses;
		unsigned char *loading_url = ses->loading_url;
		unsigned char *target_frame = ses->task.target_frame;

		ses->loading_url = ftl->url;
		ses->task.target_frame = ftl->target_frame;
		ses_chktype(ses, &ftl->stat, ftl->ce, 1);
		ses->loading_url = loading_url;
		ses->task.target_frame = target_frame;
	}
#if 0
		free_wtd(ftl->ses);
		reload(ses, CACHE_MODE_NORMAL);
		return;
	}
#endif

	doc_end_load(stat, ftl->ses);
}

struct file_to_load *
request_additional_file(struct session *ses, unsigned char *name, unsigned char *url, int pri)
{
	struct file_to_load *ftl;
	enum protocol protocol = known_protocol(url, NULL);

	if (protocol == PROTOCOL_UNKNOWN) {
		return NULL;
	}

	/* XXX: We cannot run the external handler here, because
	 * request_additional_file() is called many times for a single URL
	 * (normally the foreach() right below catches them all). Anyway,
	 * having <frame src="mailto:foo"> would be just weird, wouldn't it?
	 * --pasky */
	if (protocol != PROTOCOL_INVALID
	    && get_protocol_external_handler(protocol)) {
		return NULL;
	}

	foreach (ftl, ses->more_files) {
		if (!strcmp(ftl->url, url)) {
			if (ftl->pri > pri) {
				ftl->pri = pri;
				change_connection(&ftl->stat, &ftl->stat, pri, 0);
			}
			return NULL;
		}
	}

	ftl = mem_calloc(1, sizeof(struct file_to_load));
	if (!ftl) return NULL;

	ftl->url = stracpy(url);
	if (!ftl->url) {
		mem_free(ftl);
		return NULL;
	}
	ftl->target_frame = stracpy(name);

	ftl->stat.end = (void (*)(struct download *, void *)) file_end_load;
	ftl->stat.data = ftl;
	ftl->pri = pri;
	ftl->ses = ses;

	add_to_list(ses->more_files, ftl);

	return ftl;
}

#if 0
struct file_to_load *
request_additional_loading_file(struct session *ses, unsigned char *url,
				struct download *stat, int pri)
{
	struct file_to_load *ftl;

	ftl = request_additional_file(ses, url, pri);
	if (!ftl) {
		change_connection(stat, NULL, PRI_CANCEL, 0);
		return NULL;
	}

	ftl->req_sent = 1;
	ftl->ce = stat->ce;

	change_connection(stat, &ftl->stat, pri, 0);

	return ftl;
}
#endif

void
process_file_requests(struct session *ses)
{
	static int stop_recursion = 0;
	struct file_to_load *ftl;
	struct document_view *doc_view = current_frame(ses);
	int more = 1;

	if (stop_recursion) return;
	stop_recursion = 1;

	while (more) {
		more = 0;
		foreach (ftl, ses->more_files) {
			unsigned char *referer = NULL;

			if (ftl->req_sent)
				continue;

			ftl->req_sent = 1;
			if (doc_view && doc_view->document)
				referer = doc_view->document->url;

			load_url(ftl->url, referer,
				 &ftl->stat, ftl->pri, CACHE_MODE_NORMAL, -1);
			more = 1;
		}
	}

	stop_recursion = 0;
}

struct session *
create_basic_session(struct window *tab)
{
	struct session *ses = mem_calloc(1, sizeof(struct session));

	if (!ses) return NULL;

	create_history(&ses->history);
	init_list(ses->scrn_frames);
	init_list(ses->more_files);
	init_list(ses->tq);
	ses->tab = tab;
	ses->id = session_id++;
	ses->task.type = TASK_NONE;
	ses->display_timer = -1;

#ifdef CONFIG_LEDS
	init_led_panel(&ses->status.leds);
	ses->status.ssl_led = register_led(ses, 0);
#endif

	add_to_list(sessions, ses);

	return ses;
}

static void
dialog_goto_url_open_first(void *data)
{
	dialog_goto_url((struct session *) data, NULL);
	first_use = 0;
}

static struct session *
create_session(struct window *tab)
{
	struct terminal *term = tab->term;
	struct session *ses = create_basic_session(tab);

	if (!ses) return NULL;

	if (first_use) {
		msg_box(term, NULL, 0,
			N_("Welcome"), AL_CENTER,
			N_("Welcome to ELinks!\n\n"
			"Press ESC for menu. Documentation is available in "
			"Help menu."),
			ses, 1,
			N_("OK"), dialog_goto_url_open_first, B_ENTER | B_ESC);
	}

	if (!*get_opt_str("protocol.http.user_agent")) {
		msg_box(term, NULL, 0,
			N_("Warning"), AL_CENTER,
			N_("You have empty string in protocol.http.user_agent - "
			"this was a default value in the past, substituted by "
			"default ELinks User-Agent string. However, currently "
			"this means that NO User-Agent HEADER "
			"WILL BE SENT AT ALL - if this is really what you want, "
			"set its value to \" \", otherwise please delete line "
			"with this settings from your configuration file (if you "
			"have no idea what I'm talking about, just do this), so "
			"that correct default setting will be used. Apologies for "
			"any inconvience caused."),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
	}

	if (!get_opt_bool("config.saving_style_w")) {
		get_opt_bool("config.saving_style_w") = 1;
		get_opt_rec(config_options, "config.saving_style_w")->flags |= OPT_TOUCHED;
		if (get_opt_int("config.saving_style") != 3) {
			msg_box(term, NULL, 0,
				N_("Warning"), AL_CENTER,
				N_("You have option config.saving_style set to "
				"a de facto obsolete value. The configuration "
				"saving algorithms of ELinks were changed from "
				"the last time you upgraded ELinks. Now, only "
				"those options which you actually changed are "
				"saved to the configuration file, instead of "
				"just all the options. This simplifies our "
				"situation greatly when we see that some option "
				"has inappropriate default value or we need to "
				"change semantic of some option in a subtle way. "
				"Thus, we recommend you to change the value of "
				"config.saving_style option to 3 in order to get "
				"the \"right\" behaviour. Apologies for any "
				"inconvience caused."),
				NULL, 1,
				N_("OK"), NULL, B_ENTER | B_ESC);
		}
	}

	return ses;
}

static inline void
copy_session(struct session *old, struct session *new)
{
	if (!have_location(old)) return;

	goto_url(new, cur_loc(old)->vs.url);
}

void *
create_session_info(int cp, unsigned char *url, int *ll)
{
	int l = strlen(url);
	int *i;

	*ll = 2 * sizeof(int) + l;

	i = mem_alloc(*ll);
	if (!i) return NULL;

	i[0] = cp;
	i[1] = l;
	if (l) memcpy(i + 2, url, l);

	return i;
}


struct initial_session_info *
decode_session_info(const void *pdata)
{
	int *data = (int *) pdata;
	int len = *(data++);
	struct initial_session_info *info;
	int url_len;

	if (len < 2 * sizeof(int)) return NULL;

	info = mem_calloc(1, sizeof(struct initial_session_info));
	if (!info) return NULL;

	info->base_session = *(data++);

	url_len = *(data++);
	if (url_len && len >= 2 * sizeof(int) + url_len) {
		unsigned char *url = fmem_alloc(url_len + 1);

		if (!url) return info;

		memcpy(url, data, url_len);
		url[url_len] = '\0';

		info->url = decode_shell_safe_url(url);

		fmem_free(url);
	}

	return info;
}

static void
free_session_info(struct initial_session_info *info)
{
	if (info->url) mem_free(info->url);
	mem_free(info);
}

static void
dialog_goto_url_open(void *data)
{
	dialog_goto_url((struct session *) data, NULL);
}

static int
process_session_info(struct session *ses, struct initial_session_info *info)
{
	struct session *s;

	if (!info) return -1;

	/* This is the only place where s->id comes into game - we're comparing
	 * it to possibly supplied -base-session here, and clone the session
	 * with id of base-session (its current document association only,
	 * rather) to the newly created session. */
	foreach (s, sessions) {
		if (s->id == info->base_session) {
			copy_session(s, ses);
			break;
		}
	}

	if (info->url) {
		goto_url(ses, info->url);

#ifdef CONFIG_BOOKMARKS
	} else if (!first_use
		   && number_of_tabs(ses->tab->term) < 2
		   && get_opt_bool("ui.sessions.auto_restore")) {
		open_bookmark_folder(ses, get_opt_str("ui.sessions.auto_save_foldername"));

#endif
	} else {
		unsigned char *h = getenv("WWW_HOME");

		if (!h || !*h)
			h = WWW_HOME_URL;
		if (!h || !*h) {
			if (get_opt_int("ui.startup_goto_dialog")
			    && !first_use) {
				/* We can't create new window in EV_INIT
				 * handler! */
				register_bottom_half(dialog_goto_url_open, ses);
			}
		} else {
			goto_url(ses, h);
		}
	}

	free_session_info(info);
	return 0;
}

void
abort_loading(struct session *ses, int interrupt)
{
	if (have_location(ses)) {
		struct location *l = cur_loc(ses);

		if (l->download.state >= 0)
			change_connection(&l->download, NULL, PRI_CANCEL, interrupt);
		abort_files_load(ses, interrupt);
	}
	abort_preloading(ses, interrupt);
}

static void
destroy_session(struct session *ses)
{
	struct document_view *doc_view;
	struct tq *tq;

	assert(ses);
	if_assert_failed return;

	destroy_downloads(ses);
	abort_loading(ses, 0);
	free_files(ses);
	if (ses->doc_view) {
		detach_formatted(ses->doc_view);
		mem_free(ses->doc_view);
	}

	foreach (doc_view, ses->scrn_frames)
		detach_formatted(doc_view);

	free_list(ses->scrn_frames);

	destroy_history(&ses->history);
	set_referrer(ses, NULL);

	if (ses->loading_url) mem_free(ses->loading_url);
	if (ses->display_timer != -1) kill_timer(ses->display_timer);
	if (ses->goto_position) mem_free(ses->goto_position);
	if (ses->imgmap_href_base) mem_free(ses->imgmap_href_base);
	if (ses->imgmap_target_base) mem_free(ses->imgmap_target_base);

	foreach (tq, ses->tq) {
		if (tq->ce) object_unlock(tq->ce);
		if (tq->url) {
			change_connection(&tq->download, NULL, PRI_CANCEL, 0);
			mem_free(tq->url);
		}
		if (tq->goto_position) mem_free(tq->goto_position);
		if (tq->prog) mem_free(tq->prog);
		if (tq->target_frame) mem_free(tq->target_frame);
	}
	free_list(ses->tq);

	if (ses->dn_url) mem_free(ses->dn_url);
	if (ses->search_word) mem_free(ses->search_word);
	if (ses->last_search_word) mem_free(ses->last_search_word);
	if (ses->status.last_title) mem_free(ses->status.last_title);
	del_from_list(ses);
}

void
reload(struct session *ses, enum cache_mode cache_mode)
{
	abort_loading(ses, 0);

	if (cache_mode == CACHE_MODE_INCREMENT) {
		cache_mode = CACHE_MODE_NEVER;
		if (ses->reloadlevel < CACHE_MODE_NEVER)
			cache_mode = ++ses->reloadlevel;
	} else {
		ses->reloadlevel = cache_mode;
	}

	if (have_location(ses)) {
		struct location *l = cur_loc(ses);
		struct file_to_load *ftl;
		struct document_view *doc_view = current_frame(ses);

		l->download.data = ses;
		l->download.end = (void *)doc_end_load;
		load_url(l->vs.url, ses->ref_url, &l->download, PRI_MAIN, cache_mode, -1);
		foreach (ftl, ses->more_files) {
			unsigned char *referer = NULL;

			if (ftl->req_sent && ftl->stat.state >= 0) continue;
			ftl->stat.data = ftl;
			ftl->stat.end = (void *)file_end_load;

			if (doc_view && doc_view->document)
				referer = doc_view->document->url;

			load_url(ftl->url, referer,
				 &ftl->stat, PRI_FRAME, cache_mode, -1);
		}
	}
}

#if 0
void
ses_load_notify(struct download *stat, struct session *ses)
{
	if (stat->state == S_TRANS || stat->state == S_OK) {
		stat->end = (void (*)(struct download *, void *))end_load;
		ses->wtd = WTD_NO;
		mem_free(ses->loading_url);
		if (ses->wtd == WTD_FORWARD) {
			ses_forward(ses);
		} else INTERNAL("bad ses->wtd");
		return;
	}
	if (stat->state >= 0) print_screen_status(ses);
	if (stat->state < 0) print_error_dlg(ses, stat);
}
#endif

void
print_unknown_protocol_dialog(struct session *ses)
{
	msg_box(ses->tab->term, NULL, 0,
		N_("Error"), AL_CENTER,
		N_("This URL contains a protocol not yet known by ELinks.\n"
		   "You can configure an external handler for it through options system."),
		ses, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

struct frame *
ses_find_frame(struct session *ses, unsigned char *name)
{
	struct location *loc = cur_loc(ses);
	struct frame *frame;

	assertm(have_location(ses), "ses_request_frame: no location yet");
	if_assert_failed return NULL;

	foreachback (frame, loc->frames)
		if (!strcasecmp(frame->name, name))
			return frame;

	return NULL;
}

struct frame *
ses_change_frame_url(struct session *ses, unsigned char *name,
		     unsigned char *url)
{
	struct location *loc = cur_loc(ses);
	struct frame *frame;
	size_t url_len = strlen(url);

	assertm(have_location(ses), "ses_change_frame_url: no location yet");
	if_assert_failed { return NULL; }

	foreachback (frame, loc->frames) {
		if (strcasecmp(frame->name, name)) continue;

		if (url_len > frame->vs.url_len) {
			struct document_view *doc_view;
			struct frame *new_frame = frame;

			/* struct view_state reserves 1 byte for url, so
			 * url_len is sufficient. */
			new_frame = mem_realloc(frame, sizeof(struct frame) + url_len);
			if (!new_frame) return NULL;

			new_frame->prev->next = new_frame->next->prev = new_frame;

			foreach (doc_view, ses->scrn_frames)
				if (doc_view->vs == &frame->vs)
					doc_view->vs = &new_frame->vs;

			frame = new_frame;
		}
		memcpy(frame->vs.url, url, url_len + 1);
		frame->vs.url_len = url_len;

		return frame;
	}

	return NULL;

}

void
tabwin_func(struct window *tab, struct term_event *ev, int fw)
{
	struct session *ses = tab->data;

	switch (ev->ev) {
		case EV_ABORT:
			if (ses) destroy_session(ses);
			if (!list_empty(sessions)) update_status();
			break;
		case EV_INIT:
			/* Perhaps we should call just create_base_session()
			 * and then do the rest of create_session() stuff
			 * (renamed to setup_first_session() or so) if this is
			 * the first tab. But I don't think it is so urgent.
			 * --pasky */
			ses = tab->data = create_session(tab);
			if (!ses || process_session_info(ses, (struct initial_session_info *) ev->b)) {
				register_bottom_half((void (*)(void *)) destroy_terminal, tab->term);
				return;
			}
			update_status();
			/* fall-through */
		case EV_RESIZE:
			if (!ses) break;
			draw_formatted(ses, 1);
			load_frames(ses, ses->doc_view);
			process_file_requests(ses);
			print_screen_status(ses);
			break;
		case EV_REDRAW:
			if (!ses) break;
			draw_formatted(ses, 0);
			print_screen_status(ses);
			break;
		case EV_KBD:
		case EV_MOUSE:
			if (ses && ses->tab == get_current_tab(ses->tab->term))
				send_event(ses, ev);
			break;
	}
}

/*
 * Gets the url being viewed by this session. Writes it into str.
 * A maximum of str_size bytes (including null) will be written.
 */
unsigned char *
get_current_url(struct session *ses, unsigned char *str, size_t str_size)
{
	unsigned char *here;
	size_t url_len;

	/* Not looking at anything */
	if (!have_location(ses))
		return NULL;

	here = cur_loc(ses)->vs.url;
	url_len = get_no_post_url_length(here);

	/* Ensure that the url size is not greater than str_size.
	 * We can't just happily strncpy(str, here, str_size)
	 * because we have to stop at POST_CHAR, not only at NULL. */
	int_upper_bound(&url_len, str_size - 1);

	return safe_strncpy(str, here, url_len + 1);
}

/*
 * Gets the title of the page being viewed by this session. Writes it into str.
 * A maximum of str_size bytes (including null) will be written.
 */
unsigned char *
get_current_title(struct session *ses, unsigned char *str, size_t str_size)
{
	struct document_view *doc_view = current_frame(ses);

	/* Ensure that the title is defined */
	if (doc_view && doc_view->document->title)
		return safe_strncpy(str, doc_view->document->title, str_size);

	return NULL;
}

/*
 * Gets the url of the link currently selected. Writes it into str.
 * A maximum of str_size bytes (including null) will be written.
 */
unsigned char *
get_current_link_url(struct session *ses, unsigned char *str, size_t str_size)
{
	struct link *l = get_current_link(ses);

	if (l) return safe_strncpy(str, l->where ? l->where : l->where_img,
				   str_size);

	return NULL;
}

/* get_current_link_name: returns the name of the current link
 * (the text between <A> and </A>), str is a preallocated string,
 * str_size includes the null char. */
unsigned char *
get_current_link_name(struct session *ses, unsigned char *str, size_t str_size)
{
	struct link *l = get_current_link(ses);

	if (l) return safe_strncpy(str, l->name, str_size);

	return NULL;
}

struct link *
get_current_link(struct session *ses)
{
	struct document_view *doc_view = current_frame(ses);

	if (doc_view && doc_view->vs->current_link != -1) {
		struct link *link;

		link = &doc_view->document->links[doc_view->vs->current_link];

		/* Only return a hyper text link */
		if (link->type == LINK_HYPERTEXT) return link;
	}

	return NULL;
}
