/* Sessions status managment */
/* $Id: status.c,v 1.6 2003/12/01 16:08:02 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#ifdef USE_LEDS
#include "bfu/leds.h"
#endif
#include "bfu/style.h"
#include "config/options.h"
#include "cache/cache.h"
#include "document/document.h"
#include "document/options.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
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

	if (stat->state != S_TRANS || !(stat->prg->elapsed / 100)) {

		/* debug("%d -> %s", stat->state, _(get_err_msg(stat->state), term)); */
		return stracpy(get_err_msg(stat->state, term));
	}

	if (!init_string(&msg)) return NULL;

	/* FIXME: The following is a PITA from the l10n standpoint. A *big*
	 * one, _("of")-like pearls are a nightmare. Format strings need to
	 * be introduced to this fuggy corner of code as well. --pasky */

	add_to_string(&msg, _("Received", term));
	add_char_to_string(&msg, ' ');
	add_xnum_to_string(&msg, stat->prg->pos + stat->prg->start);
	if (stat->prg->size >= 0) {
		add_char_to_string(&msg, ' ');
		add_to_string(&msg, _("of", term));
		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, stat->prg->size);
	}

	add_to_string(&msg, separator);

	if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
		add_to_string(&msg,
			      _(wide ? N_("average speed") : N_("avg"), term));
	} else {
		add_to_string(&msg, _("speed", term));
	}

	add_char_to_string(&msg, ' ');
	add_xnum_to_string(&msg, average_speed(stat->prg));
	add_to_string(&msg, "/s");

	if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
		add_to_string(&msg, ", ");
		add_to_string(&msg,
			      _(wide ? N_("current speed") : N_("cur"), term));
		add_char_to_string(&msg, ' '),
		add_xnum_to_string(&msg, current_speed(stat->prg));
		add_to_string(&msg, "/s");
	}

	if (!full) return msg.source;

	/* Do the following only if there is room */

	add_to_string(&msg, separator);

	add_to_string(&msg, _("elapsed time", term));
	add_char_to_string(&msg, ' ');
	add_time_to_string(&msg, stat->prg->elapsed);

	if (stat->prg->size >= 0 && stat->prg->loaded > 0) {
		add_to_string(&msg, ", ");
		add_to_string(&msg, _("estimated time", term));
		add_char_to_string(&msg, ' ');
		add_time_to_string(&msg, estimated_time(stat->prg));
	}

	return msg.source;
}


void
init_bars_status(struct session *ses, int *tabs_count, struct document_options *doo)
{
	static int prev_tabs_bar = 0;
	int show_tabs_bar = get_opt_int("ui.tabs.show_bar");
	int tabs_cnt = number_of_tabs(ses->tab->term);

	if (!doo && ses->doc_view && ses->doc_view->document)
		doo = &ses->doc_view->document->options;

	if (tabs_count) *tabs_count = tabs_cnt;
	ses->visible_tabs_bar = (show_tabs_bar > 0) &&
				!(show_tabs_bar == 1 && tabs_cnt < 2);

	if (prev_tabs_bar != ses->visible_tabs_bar) {
		prev_tabs_bar = ses->visible_tabs_bar;
		set_screen_dirty(ses->tab->term->screen, 0, ses->tab->term->height);
	}

	if (doo) {
		doo->x = 0;
		doo->y = 0;
		if (ses->visible_title_bar) doo->y = 1;
		doo->width = ses->tab->term->width;
		doo->height = ses->tab->term->height;
		if (ses->visible_title_bar) doo->height--;
		if (ses->visible_status_bar) doo->height--;
		if (ses->visible_tabs_bar) doo->height--;
	}
}

static inline void
display_status_bar(struct session *ses, struct terminal *term, int tabs_count)
{
	static int last_current_link;
	unsigned char *msg = NULL;
	unsigned int tab_info_len = 0;
	struct download *stat = get_current_download(ses);
	struct color_pair *text_color = NULL;

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
			int full = term->width > 100;
			int wide = term->width > 80;

			msg = get_stat_msg(stat, term, wide, full, ", ");
		}
	}

	draw_area(term, 0, term->height - 1, term->width, 1, ' ', 0,
		get_bfu_color(term, "status.status-bar"));

	if (!ses->visible_tabs_bar && tabs_count > 1) {
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

	draw_text(term, 0 + tab_info_len, term->height - 1,
		  msg, strlen(msg), 0, text_color);
	mem_free(msg);
}

static inline void
display_tab_bar(struct session *ses, struct terminal *term, int tabs_count)
{
	struct color_pair *normal_color = get_bfu_color(term, "tabs.normal");
	struct color_pair *selected_color = get_bfu_color(term, "tabs.selected");
	struct color_pair *loading_color = get_bfu_color(term, "tabs.loading");
	struct color_pair *tabsep_color = get_bfu_color(term, "tabs.separator");
	int tab_width = int_max(1, term->width / tabs_count);
	int tab_total_width = tab_width * tabs_count;
	int tab_remain_width = int_max(0, term->width - tab_total_width);
	int tab_num;
	int ypos = term->height - (ses->visible_status_bar ? 2 : 1);
	int xpos = 0;

	for (tab_num = 0; tab_num < tabs_count; tab_num++) {
		struct color_pair *color;
		struct window *tab = get_tab_by_number(term, tab_num);
		struct document_view *doc_view;
		int actual_tab_width = tab_width;
		unsigned char *msg;
		int msglen;

		/* Adjust tab size to use full term width. */
		if (tab_remain_width) {
			actual_tab_width++;
			tab_remain_width--;
			if (tab_num == tabs_count - 1) {
				actual_tab_width += tab_remain_width;
			}
		}

		doc_view = tab->data ? current_frame(tab->data) : NULL;

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
			draw_char(term, xpos, ypos, BORDER_SVLINE,
				  SCREEN_ATTR_FRAME, tabsep_color);
			xpos += 1;
		}

		/* TODO: fresh_color, for tabs that have not been
		 * selected since they completed loading. -- Miciah */
		if (tab_num == term->current_tab) {
			color = selected_color;
		} else {
			struct download *stat;

			stat = get_current_download(tab->data);

			if (stat && stat->state != S_OK)
				color = loading_color;
			else
				color = normal_color;
		}

		draw_area(term, xpos, ypos, actual_tab_width, 1, ' ', 0, color);

		msglen = int_min(strlen(msg), actual_tab_width - 1);

		draw_text(term, xpos, ypos, msg, msglen, 0, color);
		tab->xpos = xpos;
		tab->width = actual_tab_width;
		xpos += actual_tab_width;
	}
}

/* Print statusbar and titlebar, set terminal title. */
void
print_screen_status(struct session *ses)
{
	struct terminal *term = ses->tab->term;
	unsigned char *msg = NULL;
	int tabs_count;
	int ses_tab_is_current = (ses->tab == get_current_tab(ses->tab->term));

	init_bars_status(ses, &tabs_count, NULL);

	if (ses->visible_status_bar && ses_tab_is_current) {
		display_status_bar(ses, term, tabs_count);
	}

	if (ses->visible_tabs_bar) {
		display_tab_bar(ses, term, tabs_count);
	}

	if (ses_tab_is_current && ses->visible_title_bar) {
		draw_area(term, 0, 0, term->width, 1, ' ', 0,
			  get_bfu_color(term, "title.title-bar"));

		if (current_frame(ses)) {
			msg = print_current_title(ses);
			if (msg) {
				int msglen = strlen(msg);
				int pos = int_max(term->width - 1 - msglen, 0);

				draw_text(term, pos, 0, msg, msglen, 0,
					  get_bfu_color(term, "title.title-text"));
				mem_free(msg);
			}
		}
	}

	if (!ses_tab_is_current) goto title_set;
	msg = stracpy("ELinks");
	if (msg) {
		int msglen;
		static void *last_ses = NULL;

		if (ses->doc_view && ses->doc_view->document
		    && ses->doc_view->document->title
		    && ses->doc_view->document->title[0]) {
			add_to_strn(&msg, " - ");
			add_to_strn(&msg, ses->doc_view->document->title);
		}

		msglen = strlen(msg);
		if (last_ses != ses
		    || !ses->last_title
		    || strlen(ses->last_title) != msglen
		    || memcmp(ses->last_title, msg, msglen)) {
			if (ses->last_title) mem_free(ses->last_title);
			ses->last_title = msg;
			set_terminal_title(term, msg);
			last_ses = ses;
		} else {
			mem_free(msg);
		}
	}
title_set:

	redraw_from_window(ses->tab);

#ifdef USE_LEDS
	if (ses->doc_view && ses->doc_view->document
	    && ses->doc_view->document->url) {
		struct cache_entry *cache_entry =
			find_in_cache(ses->doc_view->document->url);

		if (cache_entry) {
			ses->ssl_led->value = (cache_entry->ssl_info)
					    ? 'S' : '-';
		} else {
			/* FIXME: We should do this thing better. */
			ses->ssl_led->value = '?';
		}
	}

	draw_leds(ses);
#endif
}
