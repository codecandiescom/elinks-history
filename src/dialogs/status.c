/* Sessions status managment */
/* $Id: status.c,v 1.67 2004/05/13 09:33:22 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/leds.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "config/options.h"
#include "cache/cache.h"
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
get_stat_msg(struct download *stat, struct terminal *term,
	     int wide, int full, unsigned char *separator)
{
	struct string msg;
	int newlines = separator[strlen(separator) - 1] == '\n';

	if (!download_is_progressing(stat)) {
		/* DBG("%d -> %s", stat->state, _(get_err_msg(stat->state), term)); */
		return stracpy(get_err_msg(stat->state, term));
	}

	if (!init_string(&msg)) return NULL;

	/* FIXME: The following is a PITA from the l10n standpoint. A *big*
	 * one, _("of")-like pearls are a nightmare. Format strings need to
	 * be introduced to this fuggy corner of code as well. --pasky */

	add_to_string(&msg, _("Received", term));
	add_char_to_string(&msg, ' ');
	add_xnum_to_string(&msg, stat->prg->pos);
	if (stat->prg->size >= 0) {
		add_char_to_string(&msg, ' ');
		add_to_string(&msg, _("of", term));
		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, stat->prg->size);
	}

	add_to_string(&msg, separator);

	if (wide && stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
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
	add_xnum_to_string(&msg, average_speed(stat->prg));
	add_to_string(&msg, "/s");

	if (wide && stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
		add_to_string(&msg, ", ");
		add_to_string(&msg,
			      _(full ? N_("current speed") : N_("cur"), term));
		add_char_to_string(&msg, ' '),
		add_xnum_to_string(&msg, current_speed(stat->prg));
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
		add_time_to_string(&msg, stat->prg->elapsed);
	}

	if (stat->prg->size >= 0 && stat->prg->loaded > 0) {
		if (wide) add_to_string(&msg, ", ");
		add_to_string(&msg, _(full ? N_("estimated time")
					   : N_("ETA"),
				      term));
		add_char_to_string(&msg, ' ');
		add_time_to_string(&msg, estimated_time(stat->prg));
	}

	return msg.source;
}


#define show_tabs(option, tabs) (((option) > 0) && !((option) == 1 && (tabs) < 2))

void
update_status(void)
{
	int show_title_bar = get_opt_int("ui.show_title_bar");
	int show_status_bar = get_opt_int("ui.show_status_bar");
	int show_tabs_bar = get_opt_int("ui.tabs.show_bar");
#ifdef CONFIG_LEDS
	int show_leds = get_opt_int("ui.leds.enable");
#endif
	int set_window_title = get_opt_bool("ui.window_title");
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

		if (status->show_title_bar != show_title_bar) {
			status->show_title_bar = show_title_bar;
			dirty = 1;
		}

		if (status->show_status_bar != show_status_bar) {
			status->show_status_bar = show_status_bar;
			dirty = 1;
		}

		if (show_tabs(show_tabs_bar, tabs) != status->show_tabs_bar) {
			status->show_tabs_bar = show_tabs(show_tabs_bar, tabs);
			/* Force the current document to be rerendered so the
			 * document view and document height is updated to fit
			 * into the new dimensions. Related to bug 87. */
			render_document_frames(ses);
			dirty = 1;
		}
#ifdef CONFIG_LEDS
		if (status->show_leds != show_leds) {
			status->show_leds = show_leds;
			dirty = 1;
		}
#endif

		status->set_window_title = set_window_title;

		if (!dirty) continue;

		set_screen_dirty(term->screen, 0, term->height);
	}
}

static inline void
display_status_bar(struct session *ses, struct terminal *term, int tabs_count)
{
	static int last_current_link;
	unsigned char *msg = NULL;
	unsigned int tab_info_len = 0;
	struct download *stat = get_current_download(ses);
	struct session_status *status = &ses->status;
	struct color_pair *text_color = NULL;
	int msglen;
	struct rect box;
	
	if (stat) {
		/* Show S_INTERRUPTED message *once* but then show links
		 * again as usual. */
		if (current_frame(ses)) {
			int ncl = current_frame(ses)->vs->current_link;

			if (stat->state == S_INTERRUPTED
				&& ncl != last_current_link)
				stat->state = S_OK;
			last_current_link = ncl;

			if (stat->state == S_OK)
				msg = print_current_link(ses);
		}

		if (!msg) {
			int full = term->width > 130;
			int wide = term->width > 80;

			msg = get_stat_msg(stat, term, wide, full, ", ");
		}
	}

	set_rect(&box, 0, term->height - 1, term->width, 1);
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

	if (download_is_progressing(stat) && stat->prg->size > 0) {
		int xend = term->width - 1;
		int width;

#ifdef CONFIG_LEDS
		if (ses->status.show_leds)
			xend -= LEDS_COUNT + 2;
#endif
		/* FIXME: Figure out how to substract with of the digital clock
		 * so the progress bar is not drawn on top of it. --jonas */

		if (xend - msglen < 6) return;
		width = int_min(20, xend - msglen - 1);

		download_progress_bar(term, xend - width, term->height - 1,
				      width, NULL, NULL,
				      stat->prg->pos, stat->prg->size);
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
	struct rect box;

	set_rect(&box, 0, term->height - (status->show_status_bar ? 2 : 1), 0, 1);
	
	for (tab_num = 0; tab_num < tabs_count; tab_num++) {
		struct download *stat = NULL;
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
			stat = get_current_download(tab_ses);

			if (stat && stat->state != S_OK) {
				color = loading_color;
			} else if (!tab_ses || !tab_ses->status.visited) {
				color = fresh_color;
			}

			if (!download_is_progressing(stat)
			    || stat->prg->size <= 0)
				stat = NULL;
		}

		box.width = actual_tab_width + 1;
		draw_box(term, &box, ' ', 0, color);

		if (stat) {
			download_progress_bar(term, box.x, box.y,
					      actual_tab_width, msg, NULL,
					      stat->prg->pos, stat->prg->size);
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
	unsigned char buf[80];
	int buflen = 0;
	int height;
	struct rect box;
	
	/* Clear the old title */
	set_rect(&box, 0, 0, term->width, 1);
	draw_box(term, &box, ' ', 0, get_bfu_color(term, "title.title-bar"));

	doc_view = current_frame(ses);
	if (!doc_view || !doc_view->document) return;

	if (!init_string(&title)) return;

	document = doc_view->document;

	/* Set up the document page info string: '(' %page '/' %pages ')' */
	height = doc_view->dimensions.height;
	if (height < document->height) {
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
