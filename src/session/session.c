/* Sessions managment - you'll find things here which you wouldn't expect */
/* $Id: session.c,v 1.395 2004/05/23 16:56:14 jonas Exp $ */

/* stpcpy */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

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
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/refresh.h"
#include "document/view.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "lowlevel/select.h"
#include "osdep/newwin.h"
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


struct file_to_load {
	LIST_HEAD(struct file_to_load);

	struct session *ses;
	unsigned int req_sent:1;
	int pri;
	struct cache_entry *cached;
	unsigned char *target_frame;
	struct uri *uri;
	struct download stat;
};

#define file_to_load_is_active(ftl) ((ftl)->req_sent && is_in_progress_state((ftl)->stat.state))


INIT_LIST_HEAD(sessions);

enum remote_session_flags remote_session_flags;
static int session_id = 1;


static struct file_to_load * request_additional_file(struct session *,
						unsigned char *, struct uri *, int);


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
			if (file_to_load_is_active(ftl))
				return &ftl->stat;
	}

	/* Note that @stat isn't necessarily NULL here,
	 * if @ses->more_files is empty. -- Miciah */
	return stat;
}

void
print_error_dialog(struct session *ses, enum connection_state state,
		   enum connection_priority priority)
{
	unsigned char *t = get_err_msg(state, ses->tab->term);

	/* Don't show error dialogs for missing CSS stylesheets */
	if (!t || priority == PRI_CSS) return;
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
			if (!file_to_load_is_active(ftl))
				continue;

			q = 1;
			change_connection(&ftl->stat, NULL, PRI_CANCEL, interrupt);
		}
	} while (q);
}

void
free_files(struct session *ses)
{
	struct file_to_load *ftl;

	abort_files_load(ses, 0);
	foreach (ftl, ses->more_files) {
		if (ftl->cached) object_unlock(ftl->cached);
		if (ftl->uri) done_uri(ftl->uri);
		mem_free_if(ftl->target_frame);
	}
	free_list(ses->more_files);
}




static void request_frameset(struct session *, struct frameset_desc *);

static void
request_frame(struct session *ses, unsigned char *name, struct uri *uri)
{
	struct location *loc = cur_loc(ses);
	struct frame *frame;

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

#if 0
		/* This seems not to be needed anymore, it looks like this
		 * condition should never happen. It's apparently what Mikulas
		 * thought, at least. I'll review this more carefully when I
		 * will understand this stuff better ;-). --pasky */
		if (frame->vs.view && document_has_frams(frame->vs.view->document)) {
			request_frameset(ses, frame->vs.view->document->frame_desc);
			return;
		}
#endif
		goto found;
	}

	frame = mem_calloc(1, sizeof(struct frame));
	if (!frame) return;

	frame->name = stracpy(name);
	if (!frame->name) {
		mem_free(frame);
		return;
	}

	/* If there is no fragment part we can take a shortcut */
	if (!memchr(uri->data, '#', uri->datalen)) {
		init_vs(&frame->vs, uri, -1);

	} else {
		unsigned char *pos = NULL;

		uri = get_translated_uri(struri(uri), NULL, &pos);
		if (!uri) {
			mem_free(frame->name);
			mem_free(frame);
			return;
		}

		init_vs(&frame->vs, uri, -1);
		if (pos) frame->vs.goto_position = pos;
		done_uri(uri);
	}

	add_to_list(loc->frames, frame);

found:
	request_additional_file(ses, name, frame->vs.uri, PRI_FRAME);
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
			} else if (frame_desc->name && frame_desc->uri) {
				request_frame(ses, frame_desc->name,
					      frame_desc->uri);
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

#ifdef CONFIG_CSS
static inline void
load_css_imports(struct session *ses, struct document_view *doc_view)
{
	struct document *document = doc_view->document;
	struct string_list_item *import;

	if (!document) return;

	foreach (import, document->css_imports) {
		struct uri *uri = get_uri(import->string.source, -1);

		if (!uri) continue;
		request_additional_file(ses, "", uri, PRI_CSS);
		done_uri(uri);
	}
}
#else
#define load_css_imports(ses, doc_view)
#endif

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
	load_css_imports(ses, ses->doc_view);
	process_file_requests(ses);
}


struct questions_entry {
	LIST_HEAD(struct questions_entry);

	void (*callback)(struct session *, void *);
	void *data;
};

INIT_LIST_HEAD(questions_queue);


void
check_questions_queue(struct session *ses)
{
	while (!list_empty(questions_queue)) {
		struct questions_entry *q = questions_queue.next;

		q->callback(ses, q->data);
		del_from_list(q);
		mem_free(q);
	}
}

void
add_questions_entry(void (*callback)(struct session *, void *), void *data)
{
	struct questions_entry *q = mem_alloc(sizeof(struct questions_entry));

	if (!q) return;

	q->callback = callback;
	q->data	    = data;
	add_to_list(questions_queue, q);
}

#ifdef CONFIG_SCRIPTING
static void
maybe_pre_format_html(struct cache_entry *cached, struct session *ses)
{
	struct fragment *fr;
	unsigned char *src;
	unsigned char *uri;
	int len;
	static int pre_format_html_event = EVENT_NONE;

	if (!cached || cached->preformatted || list_empty(cached->frag))
		return;

	defrag_entry(cached);
	fr = cached->frag.next;
	src = fr->data;
	len = fr->length;
	uri = get_cache_uri_string(cached);

	set_event_id(pre_format_html_event, "pre-format-html");
	trigger_event(pre_format_html_event, &src, &len, ses, uri);

	if (src && src != fr->data) {
		add_fragment(cached, 0, src, len);
		truncate_entry(cached, len, 1);
		cached->incomplete = 0; /* XXX */
		mem_free(src);
	}

	cached->preformatted = 1;
}
#endif

void
doc_end_load(struct download *stat, struct session *ses)
{
	int submit = 0;

	if (is_in_result_state(stat->state)) {
#ifdef CONFIG_SCRIPTING
		maybe_pre_format_html(stat->cached, ses);
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
		load_css_imports(ses, ses->doc_view);
		process_file_requests(ses);

		if (ses->doc_view->document->refresh
		    && get_opt_bool("document.browse.refresh")) {
			start_document_refresh(ses->doc_view->document->refresh,
					       ses);
		}

		if (stat->state != S_OK) {
			print_error_dialog(ses, stat->state, stat->pri);
		}

	} else if (ses->display_timer == -1) {
		display_timer(ses);
	}

	check_questions_queue(ses);
	print_screen_status(ses);

#ifdef CONFIG_GLOBHIST
	if (stat->conn && stat->pri != PRI_CSS) {
		unsigned char *title = ses->doc_view->document->title;
		struct uri *uri = stat->conn->uri;
		unsigned char *uristring = uri->protocol == PROTOCOL_PROXY
					 ? uri->data : struri(uri);

		add_global_history_item(uristring, title, time(NULL));
	}
#endif

	if (submit) {
		struct form_control *fc = ses->doc_view->document->forms.next;
		unsigned char *url = get_form_url(ses, ses->doc_view, fc);

		if (url) goto_link(url, fc->target, ses, 1, 0);
	}
}

static void
file_end_load(struct download *stat, struct file_to_load *ftl)
{
	if (ftl->stat.cached) {
		if (ftl->cached) object_unlock(ftl->cached);
		ftl->cached = ftl->stat.cached;
		object_lock(ftl->cached);
	}

	/* FIXME: We need to do content-type check here! However, we won't
	 * handle properly the "Choose action" dialog now :(. */
	if (ftl->cached && !ftl->cached->redirect_get && stat->pri != PRI_CSS) {
		struct session *ses = ftl->ses;
		struct uri *loading_uri = ses->loading_uri;
		unsigned char *target_frame = ses->task.target_frame;

		ses->loading_uri = ftl->uri;
		ses->task.target_frame = ftl->target_frame;
		ses_chktype(ses, &ftl->stat, ftl->cached, 1);
		ses->loading_uri = loading_uri;
		ses->task.target_frame = target_frame;
	}

	doc_end_load(stat, ftl->ses);
}

static struct file_to_load *
request_additional_file(struct session *ses, unsigned char *name, struct uri *uri, int pri)
{
	struct file_to_load *ftl;

	if (uri->protocol == PROTOCOL_UNKNOWN) {
		return NULL;
	}

	/* XXX: We cannot run the external handler here, because
	 * request_additional_file() is called many times for a single URL
	 * (normally the foreach() right below catches them all). Anyway,
	 * having <frame src="mailto:foo"> would be just weird, wouldn't it?
	 * --pasky */
	if (get_protocol_external_handler(uri->protocol)) {
		return NULL;
	}

	foreach (ftl, ses->more_files) {
		if (uris_compare(ftl->uri, uri)) {
			if (ftl->pri > pri) {
				ftl->pri = pri;
				change_connection(&ftl->stat, &ftl->stat, pri, 0);
			}
			return NULL;
		}
	}

	ftl = mem_calloc(1, sizeof(struct file_to_load));
	if (!ftl) return NULL;

	ftl->uri = get_uri_reference(uri);
	ftl->target_frame = stracpy(name);
	ftl->stat.end = (void (*)(struct download *, void *)) file_end_load;
	ftl->stat.data = ftl;
	ftl->pri = pri;
	ftl->ses = ses;

	add_to_list(ses->more_files, ftl);

	return ftl;
}


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
			struct uri *referer = NULL;

			if (ftl->req_sent)
				continue;

			ftl->req_sent = 1;
			if (doc_view && doc_view->document)
				referer = doc_view->document->uri;

			load_uri(ftl->uri, referer,
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
	init_list(ses->type_queries);
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

	goto_url(new, struri(cur_loc(old)->vs.uri));
}

/* The session info encoder and decoder:
 *
 * This is responsible for handling the initial connection between a dumb and
 * master terminal. We might be connecting to an older or newer version of
 * ELinks and has to be able to keep some kind of compatibility so that
 * everything will work as expected while being able to change the format
 * of the decoded session info. In order to avoid sending too much information
 * we use magic numbers to signal the identity of the dump client terminal.
 *
 * Magic numbers are composed by the SESSION_MAGIC() macro. It is a negative
 * magic to be able to distinguish the oldest format from the newer ones. */

#define SESSION_MAGIC(major, minor) -(((major) << 8) + (minor))

struct string *
create_session_info(struct string *info, int cp, struct list_head *url_list)
{
	int numbers[3] = { cp, SESSION_MAGIC(1, 0), remote_session_flags };
	unsigned char *number_chars = (unsigned char *) numbers;

	if (init_string(info)
	    && add_bytes_to_string(info, number_chars, sizeof(numbers))) {
		struct string_list_item *url;

		foreach (url, *url_list) {
			struct string *str = &url->string;

			add_bytes_to_string(info, str->source, str->length + 1);
		}

		return info;
	}

	done_string(info);
	return NULL;
}

struct initial_session_info *
decode_session_info(const void *pdata)
{
	int *data = (int *) pdata;
	int len = *(data++);
	struct initial_session_info *info;
	unsigned char *str;
	int magic;

	if (len < 2 * sizeof(int)) return NULL;

	info = mem_calloc(1, sizeof(struct initial_session_info));
	if (!info) return NULL;
	init_list(info->url_list);

	info->base_session = *(data++);
	magic		   = *(data++);

	switch (magic) {
	case SESSION_MAGIC(1, 0):
		/* SESSION_MAGIC(1, 0) supports multiple URIs, remote opening
		 * and magic variables:
		 *
		 *	0: base-session ID <int>
		 *	1: Session magic <int>
		 *	2: Remote <int>
		 *	3: NUL terminated URIs <unsigned char>+
		 */
		if (len < 3 * sizeof(int)) break;

		info->remote = *(data++);

		str = (unsigned char *) data;
		len -= 3 * sizeof(int);

		/* Extract multiple NUL terminated URIs */
		while (len > 0) {
			unsigned char *end = memchr(str, 0, len);

			if (!end) break;

			add_to_string_list(&info->url_list, str, end - str);

			len -= end - str + 1;
			str  = end + 1;
 		}
		return info;

	default:
		/* Older versions (up to and including 0.9.1) sends no magic
		 * variable and if this is detected we fallback to the old
		 * session info format. The format is the simplest possible
		 * one:
		 *
		 *	0: base-session ID <int>
		 *	1: URI length <int>
		 *	2: URI length bytes containing the URI <unsigned char>*
		 */
		str = (unsigned char *) data;
		len -= 2 * sizeof(int);

		if (magic <= 0 || len <= 0 || magic > len)
			return info;

		/* Extract URI containing @magic bytes */
		add_to_string_list(&info->url_list, str, magic);

		return info;
	}

	mem_free(info);
	return NULL;
}


static void
free_session_info(struct initial_session_info *info)
{
	free_string_list(&info->url_list);
	mem_free(info);
}

static void
dialog_goto_url_open(void *data)
{
	dialog_goto_url((struct session *) data, NULL);
}

unsigned char *
get_homepage_url(void)
{
	unsigned char *homepage = get_opt_str("ui.sessions.homepage");

	if (!*homepage) homepage = getenv("WWW_HOME");
	if (!homepage || !*homepage) homepage = WWW_HOME_URL;

	return homepage;
}

static int
process_session_info(struct session *ses, struct initial_session_info *info)
{
	enum term_env_type term_env = 0;
	struct session *s;

	if (!info) return -1;

	/* This is the only place where s->id comes into game - we're comparing
	 * it to possibly supplied -base-session here, and clone the session
	 * with id of base-session (its current document association only,
	 * rather) to the newly created session. */
	foreach (s, sessions) {
		/* If processing session info from a -remote instance we just
		 * want to hook up with the master. */
		if (info->remote && s->tab->term->master) {
			struct window *tab = get_current_tab(s->tab->term);

			if (can_open_in_new(s->tab->term))
				term_env = s->tab->term->environment;

			assert(tab);
			ses = tab->data;
			break;

		} else if (s->id == info->base_session) {
			copy_session(s, ses);
			break;
		}
	}

	if (!list_empty(info->url_list)) {
		int first = !info->remote || (info->remote & SES_REMOTE_CURRENT_TAB);
		struct string_list_item *str;

		foreach (str, info->url_list) {
			unsigned char *source = str->string.source;
			unsigned char *url = decode_shell_safe_url(source);

			if (!url) continue;

			if (first) {
				/* Open first url. */
				goto_url_with_hook(ses, url);
				first = 0;

			} else if (info->remote & SES_REMOTE_ADD_BOOKMARK) {
#ifdef CONFIG_BOOKMARKS
				add_bookmark(NULL, 1, url, url);
#endif
			} else if (info->remote & SES_REMOTE_NEW_WINDOW) {
				/* FIXME: Else it is quite rude because we just
				 * take the first possibility and should maybe
				 * make it possible to specify new-screen etc
				 * via -remote "openURL(..., new-*)" --jonas */
				open_url_in_new_window(ses, url, term_env);

			} else {
				/* Open next ones. */
				open_url_in_new_tab(ses, url, 1);
			}
			mem_free(url);
		}

	} else if (info->remote) {
		/* TODO: SES_REMOTE_NEW_WINDOW */
		if (info->remote & SES_REMOTE_NEW_TAB) {
			/* FIXME: This is not perfect. Doing multiple -remote
			 * with no URLs will make the goto dialogs
			 * inaccessible. Maybe we should not support this kind
			 * of thing or make the window focus detecting code
			 * more intelligent. --jonas */
			open_url_in_new_tab(ses, NULL, 0);

			if (info->remote & SES_REMOTE_PROMPT_URL) {
				/* We can't create new window in EV_INIT handler! */
				register_bottom_half(dialog_goto_url_open, ses);
			}

		} else if (info->remote & SES_REMOTE_NEW_WINDOW) {
			/* FIXME: See the url list loop */
			open_url_in_new_window(ses, NULL, term_env);

		} else if (info->remote & SES_REMOTE_PROMPT_URL) {
			/* We can't create new window in EV_INIT handler! */
			register_bottom_half(dialog_goto_url_open, ses);
		}

#ifdef CONFIG_BOOKMARKS
	} else if (!first_use
		   && number_of_tabs(ses->tab->term) < 2
		   && get_opt_bool("ui.sessions.auto_restore")) {
		open_bookmark_folder(ses, get_opt_str("ui.sessions.auto_save_foldername"));

#endif
	} else {
		unsigned char *h = get_homepage_url();

		if (!h || !*h) {
			if ((get_opt_int("ui.startup_goto_dialog")
			    && !first_use)) {
				/* We can't create new window in EV_INIT
				 * handler! */
				register_bottom_half(dialog_goto_url_open, ses);
			}
		} else {
			goto_url(ses, h);
		}
	}

	{
		/* If it is a remote session we return non zero so that the
		 * terminal of the remote session will be destroyed ASAP. */
		int remote = info->remote;

		free_session_info(info);
		return remote;
	}
}

void
abort_loading(struct session *ses, int interrupt)
{
	if (have_location(ses)) {
		struct location *l = cur_loc(ses);

		if (is_in_progress_state(l->download.state))
			change_connection(&l->download, NULL, PRI_CANCEL, interrupt);
		abort_files_load(ses, interrupt);
	}
	abort_preloading(ses, interrupt);
}

static void
destroy_session(struct session *ses)
{
	struct document_view *doc_view;

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
	set_session_referrer(ses, NULL);

	if (ses->loading_uri) done_uri(ses->loading_uri);
	if (ses->display_timer != -1) kill_timer(ses->display_timer);
	mem_free_if(ses->goto_position);
	mem_free_if(ses->imgmap_href_base);
	mem_free_if(ses->imgmap_target_base);

	while (!list_empty(ses->type_queries))
		done_type_query(ses->type_queries.next);

	if (ses->download_uri) done_uri(ses->download_uri);
	mem_free_if(ses->search_word);
	mem_free_if(ses->last_search_word);
	mem_free_if(ses->status.last_title);
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
		load_uri(l->vs.uri, ses->referrer, &l->download, PRI_MAIN, cache_mode, -1);
		foreach (ftl, ses->more_files) {
			struct uri *referer = NULL;

			if (file_to_load_is_active(ftl))
				continue;

			ftl->stat.data = ftl;
			ftl->stat.end = (void *)file_end_load;

			if (doc_view && doc_view->document)
				referer = doc_view->document->uri;

			load_uri(ftl->uri, referer,
				 &ftl->stat, ftl->pri, cache_mode, -1);
		}
	}
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
ses_change_frame_uri(struct session *ses, unsigned char *name, struct uri *uri)
{
	struct location *loc = cur_loc(ses);
	struct frame *frame;

	assertm(have_location(ses), "ses_change_frame_url: no location yet");
	if_assert_failed { return NULL; }

	foreachback (frame, loc->frames) {
		if (strcasecmp(frame->name, name)) continue;

		done_uri(frame->vs.uri);
		frame->vs.uri = get_uri_reference(uri);

		return frame;
	}

	return NULL;

}

void
set_session_referrer(struct session *ses, struct uri *referrer)
{
	if (ses->referrer) done_uri(ses->referrer);

	if (referrer && referrer->protocol != PROTOCOL_FILE) {
		/* Don't set referrer for file protocol */
		ses->referrer = get_uri_reference(referrer);
	} else {
		ses->referrer = NULL;
	}
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
	int url_len;

	/* Not looking at anything */
	if (!have_location(ses))
		return NULL;

	here = struri(cur_loc(ses)->vs.uri);
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
	struct link *l = get_current_session_link(ses);

	if (!l) return NULL;

	return safe_strncpy(str, l->where ? l->where : l->where_img, str_size);
}

/* get_current_link_name: returns the name of the current link
 * (the text between <A> and </A>), str is a preallocated string,
 * str_size includes the null char. */
unsigned char *
get_current_link_name(struct session *ses, unsigned char *str, size_t str_size)
{
	struct link *link = get_current_session_link(ses);
	unsigned char *where, *name = NULL;

	if (!link) return NULL;

	where = link->where ? link->where : link->where_img;
#ifdef CONFIG_GLOBHIST
	{
		struct global_history_item *item;

		item = get_global_history_item(where);
		if (item) name = item->title;
	}
#endif
	if (!name) name = link->name ? link->name : where;

	return safe_strncpy(str, name, str_size);
}

struct link *
get_current_link_in_view(struct document_view *doc_view)
{
	struct link *link = get_current_link(doc_view);

	return (link && (link->type == LINK_HYPERTEXT || link->type == LINK_MAP))
		? link : NULL;
}

struct link *
get_current_session_link(struct session *ses)
{
	return get_current_link_in_view(current_frame(ses));
}
