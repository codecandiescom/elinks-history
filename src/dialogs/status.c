/* Sessions status managment */
/* $Id: status.c,v 1.100 2005/04/17 15:59:54 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "config/options.h"
#include "dialogs/download.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/renderer.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/error.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"


#define average_speed(progress) \
	((longlong) (progress)->loaded * 10 / ((progress)->elapsed / 100))

#define current_speed(progress) \
	((progress)->cur_loaded / (CURRENT_SPD_SEC * SPD_DISP_TIME / 1000))

#define estimated_time(progress) \
	(((progress)->size - (progress)->pos) \
	 / ((longlong) (progress)->loaded * 10 / ((progress)->elapsed / 100)) \
	 * 1000)

unsigned char *
get_download_msg(struct download *download, struct terminal *term,
	     int wide, int full, unsigned char *separator)
{
	struct string msg;
	int newlines = separator[strlen(separator) - 1] == '\n';

	if (!download_is_progressing(download)) {
		/* DBG("%d -> %s", download->state, _(get_err_msg(download->state), term)); */
		return stracpy(get_err_msg(download->state, term));
	}

	if (!init_string(&msg)) return NULL;

	/* FIXME: The following is a PITA from the l10n standpoint. A *big*
	 * one, _("of")-like pearls are a nightmare. Format strings need to
	 * be introduced to this fuggy corner of code as well. --pasky */

	add_to_string(&msg, _("Received", term));
	add_char_to_string(&msg, ' ');
	add_xnum_to_string(&msg, download->progress->pos);
	if (download->progress->size >= 0) {
		add_char_to_string(&msg, ' ');
		add_to_string(&msg, _("of", term));
		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, download->progress->size);
	}

	add_to_string(&msg, separator);

	if (wide && download->progress->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
		add_to_string(&msg,
			      _(full ? (newlines ? N_("Average speed")
					         : N_("average speed"))
				     : N_("avg"),
				term));
	} else {
		add_to_string(&msg, _(newlines ? N_("Speed") : N_("speed"),
					term));
	}

	add_char_to_string(&msg, ' ');
	add_xnum_to_string(&msg, average_speed(download->progress));
	add_to_string(&msg, "/s");

	if (wide && download->progress->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
		add_to_string(&msg, ", ");
		add_to_string(&msg,
			      _(full ? N_("current speed") : N_("cur"), term));
		add_char_to_string(&msg, ' '),
		add_xnum_to_string(&msg, current_speed(download->progress));
		add_to_string(&msg, "/s");
	}

	if (wide) {
		/* Do the following only if there is room */

		add_to_string(&msg, separator);

		add_to_string(&msg, _(full ? (newlines ? N_("Elapsed time")
						       : N_("elapsed time"))
					   : N_("ETT"),
				   term));
		add_char_to_string(&msg, ' ');
		add_time_to_string(&msg, download->progress->elapsed);
	}

	if (download->progress->size >= 0 && download->progress->loaded > 0) {
		add_to_string(&msg, ", ");
		add_to_string(&msg, _(full ? N_("estimated time")
					   : N_("ETA"),
				      term));
		add_char_to_string(&msg, ' ');
		add_time_to_string(&msg, estimated_time(download->progress));
	}

	return msg.source;
}

int
download_is_progressing(struct download *download)
{
	return download
	    && download->state == S_TRANS
	    && download->progress->elapsed / 100;
}


#define show_tabs(option, tabs) (((option) > 0) && !((option) == 1 && (tabs) < 2))

void
update_status(void)
{
	int show_title_bar = get_opt_bool("ui.show_title_bar");
	int show_status_bar = get_opt_bool("ui.show_status_bar");
	int show_tabs_bar = get_opt_int("ui.tabs.show_bar");
#ifdef CONFIG_LEDS
	int show_leds = get_opt_bool("ui.leds.enable");
#endif
	int set_window_title = get_opt_bool("ui.window_title");
	int insert_mode = get_opt_bool("document.browse.forms.insert_mode");
	struct session *ses;
	int tabs = 1;
	struct terminal *term = NULL;

	foreach (ses, sessions) {
		struct session_status *status = &ses->status;
		int dirty = 0;

		/* Try to descrease the number of tab calculation using that
		 * tab sessions share the same term. */
		if (ses->tab->term != term) {
			term = ses->tab->term;
			tabs = number_of_tabs(term);
		}

		if (status->force_show_title_bar >= 0)
			show_title_bar = status->force_show_title_bar;
		if (status->show_title_bar != show_title_bar) {
			status->show_title_bar = show_title_bar;
			dirty = 1;
		}

		if (status->force_show_status_bar >= 0)
			show_status_bar = status->force_show_status_bar;
		if (status->show_status_bar != show_status_bar) {
			status->show_status_bar = show_status_bar;
			dirty = 1;
		}

		if (show_tabs(show_tabs_bar, tabs) != status->show_tabs_bar) {
			status->show_tabs_bar = show_tabs(show_tabs_bar, tabs);
			dirty = 1;
		}
#ifdef CONFIG_LEDS
		if (status->show_leds != show_leds) {
			status->show_leds = show_leds;
			dirty = 1;
		}
#endif

		status->set_window_title = set_window_title;

		/* This more belongs to the current browsing state but ... */
		if (!insert_mode)
			ses->insert_mode = INSERT_MODE_LESS;
		else if (ses->insert_mode == INSERT_MODE_LESS)
			ses->insert_mode = INSERT_MODE_OFF;

		if (!dirty) continue;

		/* Force the current document to be rerendered so the
		 * document view and document height is updated to fit
		 * into the new dimensions. Related to bug 87. */
		render_document_frames(ses, 1);

		set_screen_dirty(term->screen, 0, term->height);
	}
}

static unsigned char *
get_current_link_info_and_title(struct session *ses,
				struct document_view *doc_view)
{
	unsigned char *link_info, *link_title, *ret = NULL;

	link_info = get_current_link_info(ses, doc_view);
	if (!link_info) return NULL;

	link_title = get_current_link_title(doc_view);
	if (link_title) {
		assert(*link_title);
		ret = straconcat(link_info, " - ", link_title, NULL);
		mem_free(link_info);
		mem_free(link_title);
	}

	if (!ret) ret = link_info;

	return ret;
}

static inline void
display_status_bar(struct session *ses, struct terminal *term, int tabs_count)
{
	unsigned char *msg = NULL;
	unsigned int tab_info_len = 0;
	struct download *download = get_current_download(ses);
	struct session_status *status = &ses->status;
	struct color_pair *text_color = NULL;
	int msglen;
	struct box box;

#ifdef CONFIG_MARKS
	if (ses->kbdprefix.mark != KP_MARK_NOTHING) {
		switch (ses->kbdprefix.mark) {
			case KP_MARK_NOTHING:
				assert(0);
				break;

			case KP_MARK_SET:
				msg = msg_text(term, N_("Enter a mark to set"));
				break;

			case KP_MARK_GOTO:
				msg = msg_text(term, N_("Enter a mark"
							" to which to jump"));
				break;
		}
	} else
#endif
	if (ses->kbdprefix.repeat_count) {
		msg = msg_text(term, N_("Keyboard prefix: %d"),
			       ses->kbdprefix.repeat_count);
	} else if (download) {
		struct document_view *doc_view = current_frame(ses);

		/* Show S_INTERRUPTED message *once* but then show links
		 * again as usual. */
		/* doc_view->vs may be NULL here in the short interval between
		 * ses_forward() with @loading_in_frame set, disconnecting the
		 * doc_view from vs, and render_document_frames(), detaching
		 * the doc_view. */
		if (doc_view && doc_view->vs) {
			static int last_current_link;
			int ncl = doc_view->vs->current_link;

			if (download->state == S_INTERRUPTED
			    && ncl != last_current_link)
				download->state = S_OK;
			last_current_link = ncl;

			if (download->state == S_OK) {
				if (get_current_link(doc_view)) {
					msg = get_current_link_info_and_title(ses, doc_view);
				} else if (ses->navigate_mode == NAVIGATE_CURSOR_ROUTING) {
					msg = msg_text(term, N_("Cursor position: %dx%d"),
							ses->tab->x + 1, ses->tab->y + 1);
				}
			}
		}

		if (!msg) {
			/* FIXME: improve that, values should depend on
			 * context (leds, digital clock, ...). --Zas */
			int full = term->width > 130;
			int wide = term->width > 80;

			msg = get_download_msg(download, term, wide, full, ", ");
		}
	}

	set_box(&box, 0, term->height - 1, term->width, 1);
	draw_box(term, &box, ' ', 0, get_bfu_color(term, "status.status-bar"));

	if (!status->show_tabs_bar && tabs_count > 1) {
		unsigned char tab_info[8];

		tab_info[tab_info_len++] = '[';
		ulongcat(tab_info, &tab_info_len, term->current_tab + 1, 4, 0);
		tab_info[tab_info_len++] = ']';
		tab_info[tab_info_len++] = ' ';
		tab_info[tab_info_len] = '\0';

		text_color = get_bfu_color(term, "status.status-text");
		draw_text(term, 0, term->height - 1, tab_info, tab_info_len,
			0, text_color);
	}

	if (!msg) return;

	if (!text_color)
		text_color = get_bfu_color(term, "status.status-text");

	msglen = strlen(msg);
	draw_text(term, 0 + tab_info_len, term->height - 1,
		  msg, msglen, 0, text_color);
	mem_free(msg);

	if (download_is_progressing(download) && download->progress->size > 0) {
		int xend = term->width - 1;
		int width;

#ifdef CONFIG_LEDS
		if (ses->status.show_leds)
			xend -= term->leds_length;
#endif

		width = int_max(0, xend - msglen - tab_info_len - 1);
		if (width < 6) return;
		int_upper_bound(&width, 20);
		download_progress_bar(term, xend - width, term->height - 1,
				      width, NULL, NULL,
				      download->progress->pos, download->progress->size);
	}
}

static inline void
display_tab_bar(struct session *ses, struct terminal *term, int tabs_count)
{
	struct color_pair *normal_color = get_bfu_color(term, "tabs.normal");
	struct color_pair *selected_color = get_bfu_color(term, "tabs.selected");
	struct color_pair *loading_color = get_bfu_color(term, "tabs.loading");
	struct color_pair *fresh_color = get_bfu_color(term, "tabs.unvisited");
	struct color_pair *tabsep_color = get_bfu_color(term, "tabs.separator");
	struct session_status *status = &ses->status;
	int tab_width = int_max(1, term->width / tabs_count);
	int tab_total_width = tab_width * tabs_count;
	int tab_remain_width = int_max(0, term->width - tab_total_width);
	int tab_add = int_max(1, (tab_remain_width / tabs_count));
	int tab_num;
	struct box box;

	set_box(&box, 0, term->height - (status->show_status_bar ? 2 : 1), 0, 1);

	for (tab_num = 0; tab_num < tabs_count; tab_num++) {
		struct download *download = NULL;
		struct color_pair *color = normal_color;
		struct window *tab = get_tab_by_number(term, tab_num);
		struct document_view *doc_view;
		struct session *tab_ses = tab->data;
		int actual_tab_width = tab_width - 1;
		unsigned char *msg;

		/* Adjust tab size to use full term width. */
		if (tab_remain_width) {
			actual_tab_width += tab_add;
			tab_remain_width -= tab_add;
		}

		doc_view = tab_ses ? current_frame(tab_ses) : NULL;

		if (doc_view) {
			if (doc_view->document->title
			    && *(doc_view->document->title))
				msg = doc_view->document->title;
			else
				msg = _("Untitled", term);
		} else {
			msg = _("No document", term);
		}

		if (tab_num) {
			draw_char(term, box.x, box.y, BORDER_SVLINE,
				  SCREEN_ATTR_FRAME, tabsep_color);
			box.x++;
		}

		if (tab_num == term->current_tab) {
			color = selected_color;

		} else {
			download = get_current_download(tab_ses);

			if (download && download->state != S_OK) {
				color = loading_color;
			} else if (!tab_ses || !tab_ses->status.visited) {
				color = fresh_color;
			}

			if (!download_is_progressing(download)
			    || download->progress->size <= 0)
				download = NULL;
		}

		box.width = actual_tab_width + 1;
		draw_box(term, &box, ' ', 0, color);

		if (download) {
			download_progress_bar(term, box.x, box.y,
					      actual_tab_width, msg, NULL,
					      download->progress->pos, download->progress->size);
		} else {
			int msglen = int_min(strlen(msg), actual_tab_width);

			draw_text(term, box.x, box.y, msg, msglen, 0, color);
		}

		tab->xpos = box.x;
		tab->width = actual_tab_width;
		if (tab_num == tabs_count - 1) {
			/* This is the last tab, and is therefore followed
			 * by a space, not a separator; increment tab->width
			 * to count that space as part of the tab.
			 * -- Miciah */
			tab->width++;
		}

		box.x += actual_tab_width;
	}
}

/* Print page's title and numbering at window top. */
static inline void
display_title_bar(struct session *ses, struct terminal *term)
{
	struct document_view *doc_view;
	struct document *document;
	struct string title;
	unsigned char buf[40];
	int buflen = 0;
	int height;
	struct box box;

	/* Clear the old title */
	set_box(&box, 0, 0, term->width, 1);
	draw_box(term, &box, ' ', 0, get_bfu_color(term, "title.title-bar"));

	doc_view = current_frame(ses);
	if (!doc_view || !doc_view->document) return;

	if (!init_string(&title)) return;

	document = doc_view->document;

	/* Set up the document page info string: '(' %page '/' %pages ')' */
	height = doc_view->box.height;
	if (height < document->height && doc_view->vs) {
		int pos = doc_view->vs->y + height;
		int page = 1;
		int pages = height ? (document->height + height - 1) / height : 1;

		/* Check if at the end else calculate the page. */
		if (pos >= document->height) {
			page = pages;
		} else if (height) {
			page = int_min((pos - height / 2) / height + 1, pages);
		}

		buflen = snprintf(buf, sizeof(buf), " (%d/%d)", page, pages);
		if (buflen < 0) buflen = 0;
	}

	if (document->title) {
		int maxlen = int_max(term->width - 4 - buflen, 0);
		int titlelen = int_min(strlen(document->title), maxlen);

		add_bytes_to_string(&title, document->title, titlelen);

		if (titlelen == maxlen)
			add_bytes_to_string(&title, "...", 3);
	}

	if (buflen > 0)
		add_bytes_to_string(&title, buf, buflen);

	if (title.length) {
		int x = int_max(term->width - 1 - title.length, 0);

		draw_text(term, x, 0, title.source, title.length, 0,
			  get_bfu_color(term, "title.title-text"));
	}

	done_string(&title);
}

static inline void
display_window_title(struct session *ses, struct terminal *term)
{
	static struct session *last_ses;
	struct session_status *status = &ses->status;
	unsigned char *doc_title = NULL;
	unsigned char *title;
	int titlelen;

	if (ses->doc_view
	    && ses->doc_view->document
	    && ses->doc_view->document->title
	    && ses->doc_view->document->title[0])
		doc_title = ses->doc_view->document->title;

	title = straconcat("ELinks",
			   doc_title ? " - " : NULL,
			   doc_title,
			   NULL);
	if (!title) return;

	titlelen = strlen(title);
	if (last_ses != ses
	    || !status->last_title
	    || strlen(status->last_title) != titlelen
	    || memcmp(status->last_title, title, titlelen)) {
		mem_free_set(&status->last_title, title);
		set_terminal_title(term, title);
		last_ses = ses;
	} else {
		mem_free(title);
	}
}

#ifdef CONFIG_LEDS
static inline void
display_leds(struct session *ses, struct session_status *status)
{
	if (ses->doc_view && ses->doc_view->document
	    && ses->doc_view->document->uri) {
		struct cache_entry *cached =
			find_in_cache(ses->doc_view->document->uri);

		if (cached) {
			status->ssl_led->value = (cached->ssl_info)
					    ? 'S' : '-';
		} else {
			/* FIXME: We should do this thing better. */
			status->ssl_led->value = '?';
		}
	}

	if (ses->insert_mode == INSERT_MODE_LESS) {
		status->insert_mode_led->value = 'i';
	} else {
		unsigned char value = ses->insert_mode == INSERT_MODE_ON
				    ? 'I' : '-';
		status->insert_mode_led->value = value;
	}

	draw_leds(ses);
}
#endif /* CONFIG_LEDS */

/* Print statusbar and titlebar, set terminal title. */
void
print_screen_status(struct session *ses)
{
	struct terminal *term = ses->tab->term;
	struct session_status *status = &ses->status;
	int tabs_count = number_of_tabs(term);
	int ses_tab_is_current = (ses->tab == get_current_tab(term));

	if (ses_tab_is_current) {
		if (status->set_window_title)
			display_window_title(ses, term);

		if (status->show_title_bar)
			display_title_bar(ses, term);

		if (status->show_status_bar)
			display_status_bar(ses, term, tabs_count);
#ifdef CONFIG_LEDS
		if (status->show_leds)
			display_leds(ses, status);
#endif

		if (!ses->status.visited)
			ses->status.visited = 1;
	}

	if (status->show_tabs_bar) {
		display_tab_bar(ses, term, tabs_count);
	}

	redraw_from_window(ses->tab);
}

void
download_progress_bar(struct terminal *term, int x, int y, int width,
		      unsigned char *text, struct color_pair *meter_color,
		      longlong current, longlong total)
{
	/* Note : values > 100% are theorically possible and were seen. */
	int progress = (int) ((longlong) 100 * current / total);
	struct box barprogress;

	/* Draw the progress meter part "[###    ]" */
	if (!text && width > 2) {
		width -= 2;
		draw_text(term, x++, y, "[", 1, 0, NULL);
		draw_text(term, x + width, y, "]", 1, 0, NULL);
	}

	if (!meter_color) meter_color = get_bfu_color(term, "dialog.meter");
	set_box(&barprogress,
		x, y, int_min(width * progress / 100, width), 1);
	draw_box(term, &barprogress, ' ', 0, meter_color);

	/* On error, will print '?' only, should not occur. */
	if (text) {
		width = int_min(width, strlen(text));

	} else if (width > 1) {
		static unsigned char percent[] = "????"; /* Reduce or enlarge at will. */
		unsigned int percent_len = 0;
		int max = int_min(sizeof(percent), width) - 1;

		if (ulongcat(percent, &percent_len, progress, max, 0)) {
			percent[0] = '?';
			percent_len = 1;
		}

		percent[percent_len++] = '%';

		/* Draw the percentage centered in the progress meter */
		x += (1 + width - percent_len) / 2;

		assert(percent_len <= width);
		width = percent_len;
		text = percent;
	}

	draw_text(term, x, y, text, width, 0, NULL);
}
