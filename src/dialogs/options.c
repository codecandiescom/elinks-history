/* Options dialogs */
/* $Id: options.c,v 1.5 2002/04/27 13:15:52 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <links.h>

#include <main.h>
#include <bfu/align.h>
#include <bfu/bfu.h>
#include <bfu/menu.h>
#include <config/conf.h>
#include <config/options.h>
#include <dialogs/options.h>
#include <document/download.h>
#include <document/options.h>
#include <document/session.h>
#include <document/view.h>
#include <document/html/renderer.h>
#include <intl/charsets.h>
#include <intl/language.h>
#include <lowlevel/kbd.h>
#include <lowlevel/sched.h>
#include <lowlevel/select.h>
#include <lowlevel/terminal.h>
#include <protocol/types.h>
#include <util/memlist.h>


void
display_codepage(struct terminal *term, void *pcp, struct session *ses)
{
	int cp = (int)pcp;
	struct term_spec *t = new_term_spec(term->term);
	if (t) t->charset = cp;
	cls_redraw_all_terminals();
}

void
charset_list(struct terminal *term, void *xxx, struct session *ses)
{
	int i, sel;
	unsigned char *n;
	struct menu_item *mi;
	if (!(mi = new_menu(1))) return;
	for (i = 0; (n = get_cp_name(i)); i++) {
		if (is_cp_special(i)) continue;
		add_to_menu(&mi, get_cp_name(i), "", "", MENU_FUNC display_codepage, (void *)i, 0);
	}
	sel = ses->term->spec->charset;
	if (sel < 0) sel = 0;
	do_menu_selected(term, mi, ses, sel);
}

void
set_val(struct terminal *term, void *ip, int *d)
{
	*d = (int)ip;
}

void
charset_sel_list(struct terminal *term, struct session *ses, int *ptr)
{
	int i, sel;
	unsigned char *n;
	struct menu_item *mi;
	if (!(mi = new_menu(1))) return;
	for (i = 0; (n = get_cp_name(i)); i++) {
		add_to_menu(&mi, get_cp_name(i), "", "", MENU_FUNC set_val, (void *)i, 0);
	}
	sel = *ptr;
	if (sel < 0) sel = 0;
	do_menu_selected(term, mi, ptr, sel);
}


void
terminal_options_ok(void *p)
{
	cls_redraw_all_terminals();
}

unsigned char *td_labels[] = {
	TEXT(T_NO_FRAMES),
	TEXT(T_VT_100_FRAMES),
	TEXT(T_LINUX_OR_OS2_FRAMES),
	TEXT(T_KOI8R_FRAMES),
	TEXT(T_USE_11M),
	TEXT(T_RESTRICT_FRAMES_IN_CP850_852),
	TEXT(T_BLOCK_CURSOR),
	TEXT(T_COLOR),
	TEXT(T_UTF_8_IO),
	NULL
};

void
terminal_options(struct terminal *term, void *xxx, struct session *ses)
{
	struct dialog *d;
	struct term_spec *ts = new_term_spec(term->term);

	if (!ts) return;

	d = mem_alloc(sizeof(struct dialog) + 12 * sizeof(struct dialog_item));
	if (!d) return;
	memset(d, 0, sizeof(struct dialog) + 12 * sizeof(struct dialog_item));

	d->title = TEXT(T_TERMINAL_OPTIONS);
	d->fn = checkbox_list_fn;
	d->udata = td_labels;
	d->refresh = (void (*)(void *)) terminal_options_ok;

	d->items[0].type = D_CHECKBOX;
	d->items[0].gid = 1;
	d->items[0].gnum = TERM_DUMB;
	d->items[0].dlen = sizeof(int);
	d->items[0].data = (void *) &ts->mode;

	d->items[1].type = D_CHECKBOX;
	d->items[1].gid = 1;
	d->items[1].gnum = TERM_VT100;
	d->items[1].dlen = sizeof(int);
	d->items[1].data = (void *) &ts->mode;

	d->items[2].type = D_CHECKBOX;
	d->items[2].gid = 1;
	d->items[2].gnum = TERM_LINUX;
	d->items[2].dlen = sizeof(int);
	d->items[2].data = (void *) &ts->mode;

	d->items[3].type = D_CHECKBOX;
	d->items[3].gid = 1;
	d->items[3].gnum = TERM_KOI8;
	d->items[3].dlen = sizeof(int);
	d->items[3].data = (void *) &ts->mode;

	d->items[4].type = D_CHECKBOX;
	d->items[4].gid = 0;
	d->items[4].dlen = sizeof(int);
	d->items[4].data = (void *) &ts->m11_hack;

	d->items[5].type = D_CHECKBOX;
	d->items[5].gid = 0;
	d->items[5].dlen = sizeof(int);
	d->items[5].data = (void *) &ts->restrict_852;

	d->items[6].type = D_CHECKBOX;
	d->items[6].gid = 0;
	d->items[6].dlen = sizeof(int);
	d->items[6].data = (void *) &ts->block_cursor;

	d->items[7].type = D_CHECKBOX;
	d->items[7].gid = 0;
	d->items[7].dlen = sizeof(int);
	d->items[7].data = (void *) &ts->col;

	d->items[8].type = D_CHECKBOX;
	d->items[8].gid = 0;
	d->items[8].dlen = sizeof(int);
	d->items[8].data = (void *) &ts->utf_8_io;

	d->items[9].type = D_BUTTON;
	d->items[9].gid = B_ENTER;
	d->items[9].fn = ok_dialog;
	d->items[9].text = TEXT(T_OK);

	d->items[10].type = D_BUTTON;
	d->items[10].gid = B_ESC;
	d->items[10].fn = cancel_dialog;
	d->items[10].text = TEXT(T_CANCEL);

	d->items[11].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}


unsigned char *http_labels[] = {
	TEXT(T_USE_HTTP_10),
	TEXT(T_ALLOW_SERVER_BLACKLIST),
	TEXT(T_BROKEN_302_REDIRECT),
	TEXT(T_NO_KEEPALIVE_AFTER_POST_REQUEST),
	TEXT(T_REFERER_NONE),
	TEXT(T_REFERER_SAME_URL),
	TEXT(T_REFERER_FAKE),
	TEXT(T_REFERER_TRUE),
	TEXT(T_FAKE_REFERER),
	TEXT(T_USER_AGENT),
	NULL
};

void
httpopt_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;

	checkboxes_width(term, dlg->dlg->udata, &max, max_text_width);
	checkboxes_width(term, dlg->dlg->udata, &min, min_text_width);
	max_text_width(term, http_labels[8], &max);
	min_text_width(term, http_labels[8], &min);
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);
	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 5) w = 5;
	rw = 0;
	dlg_format_checkboxes(NULL, term, dlg->items, dlg->n - 4, 0, &y, w, &rw, dlg->dlg->udata);
	y++;
	dlg_format_text(NULL, term, http_labels[8], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y+=2;
	dlg_format_text(NULL, term, http_labels[9], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y+=2;
	dlg_format_buttons(NULL, term, dlg->items + dlg->n - 2, 2, 0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB + 1;
	dlg_format_checkboxes(term, term, dlg->items, dlg->n - 4, dlg->x + DIALOG_LB, &y, w, NULL, dlg->dlg->udata);
	y++;
	dlg_format_text(term, term, http_labels[8], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, dlg->items + 8, dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term, http_labels[9], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, dlg->items + 9, dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, dlg->items + dlg->n - 2, 2, dlg->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}


int
dlg_http_options(struct dialog_data *dlg, struct dialog_item_data *di)
{
	struct http_bugs *bugs = (struct http_bugs *)di->cdata;
	struct dialog *d;

	d = mem_alloc(sizeof(struct dialog) + 13 * sizeof(struct dialog_item));
	if (!d) return 0;
	memset(d, 0, sizeof(struct dialog) + 13 * sizeof(struct dialog_item));

	d->title = TEXT(T_HTTP_BUG_WORKAROUNDS);
	d->fn = httpopt_fn;
	d->udata = http_labels;

	d->items[0].type = D_CHECKBOX;
	d->items[0].gid = 0;
	d->items[0].dlen = sizeof(int);
	d->items[0].data = (void *) &bugs->http10;

	d->items[1].type = D_CHECKBOX;
	d->items[1].gid = 0;
	d->items[1].dlen = sizeof(int);
	d->items[1].data = (void *) &bugs->allow_blacklist;

	d->items[2].type = D_CHECKBOX;
	d->items[2].gid = 0;
	d->items[2].dlen = sizeof(int);
	d->items[2].data = (void *) &bugs->bug_302_redirect;

	d->items[3].type = D_CHECKBOX;
	d->items[3].gid = 0;
	d->items[3].dlen = sizeof(int);
	d->items[3].data = (void *) &bugs->bug_post_no_keepalive;

	d->items[4].type = D_CHECKBOX;
	d->items[4].gid = 1;
	d->items[4].gnum = REFERER_NONE;
	d->items[4].dlen = sizeof(int);
	d->items[4].data = (void *) &referer;

	d->items[5].type = D_CHECKBOX;
	d->items[5].gid = 1;
	d->items[5].gnum = REFERER_SAME_URL;
	d->items[5].dlen = sizeof(int);
	d->items[5].data = (void *) &referer;

	/* This should be last, but I did it wrong originally and now I would
	 * break backwards compatibility by changing it :/. */
	d->items[6].type = D_CHECKBOX;
	d->items[6].gid = 1;
	d->items[6].gnum = REFERER_FAKE;
	d->items[6].dlen = sizeof(int);
	d->items[6].data = (void *) &referer;

	d->items[7].type = D_CHECKBOX;
	d->items[7].gid = 1;
	d->items[7].gnum = REFERER_TRUE;
	d->items[7].dlen = sizeof(int);
	d->items[7].data = (void *) &referer;

	d->items[8].type = D_FIELD;
	d->items[8].dlen = MAX_STR_LEN;
	d->items[8].data = fake_referer;

	d->items[9].type = D_FIELD;
	d->items[9].dlen = MAX_STR_LEN;
	d->items[9].data = user_agent;

	d->items[10].type = D_BUTTON;
	d->items[10].gid = B_ENTER;
	d->items[10].fn = ok_dialog;
	d->items[10].text = TEXT(T_OK);
	d->items[11].type = D_BUTTON;
	d->items[11].gid = B_ESC;
	d->items[11].fn = cancel_dialog;
	d->items[11].text = TEXT(T_CANCEL);
	d->items[12].type = D_END;

	do_dialog(dlg->win->term, d, getml(d, NULL));

	return 0;
}

int
dlg_ftp_options(struct dialog_data *dlg, struct dialog_item_data *di)
{
	struct dialog *d;

	d = mem_alloc(sizeof(struct dialog) + 4 * sizeof(struct dialog_item));
	if (!d) return 0;
	memset(d, 0, sizeof(struct dialog) + 4 * sizeof(struct dialog_item));

	d->title = TEXT(T_FTP_OPTIONS);
	d->fn = input_field_fn;
	d->udata = TEXT(T_PASSWORD_FOR_ANONYMOUS_LOGIN);

	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = di->cdata;

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = ok_dialog;
	d->items[1].text = TEXT(T_OK);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ESC;
	d->items[2].fn = cancel_dialog;
	d->items[2].text = TEXT(T_CANCEL);

	d->items[3].type = D_END;

	do_dialog(dlg->win->term, d, getml(d, NULL));

	return 0;
}


unsigned char max_c_str[3];
unsigned char max_cth_str[2];
unsigned char max_t_str[2];
unsigned char time_str[5];
unsigned char unrtime_str[5];

void
refresh_net(void *xxx)
{
	/* abort_all_connections(); */
	max_connections = atoi(max_c_str);
	max_connections_to_host = atoi(max_cth_str);
	max_tries = atoi(max_t_str);
	receive_timeout = atoi(time_str);
	unrestartable_receive_timeout = atoi(unrtime_str);
	register_bottom_half((void (*)(void *))check_queue, NULL);
}

unsigned char *net_msg[] = {
	TEXT(T_HTTP_PROXY__HOST_PORT),
	TEXT(T_FTP_PROXY__HOST_PORT),
	TEXT(T_MAX_CONNECTIONS),
	TEXT(T_MAX_CONNECTIONS_TO_ONE_HOST),
	TEXT(T_RETRIES),
	TEXT(T_RECEIVE_TIMEOUT_SEC),
	TEXT(T_TIMEOUT_WHEN_UNRESTARTABLE),
	TEXT(T_ASYNC_DNS_LOOKUP),
	TEXT(T_SET_TIME_OF_DOWNLOADED_FILES),
	"",
	"",
	NULL
};

void
netopt_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;

	max_text_width(term, net_msg[0], &max);
	min_text_width(term, net_msg[0], &min);
	max_text_width(term, net_msg[1], &max);
	min_text_width(term, net_msg[1], &min);
	max_group_width(term, net_msg + 2, dlg->items + 2, 9, &max);
	min_group_width(term, net_msg + 2, dlg->items + 2, 9, &min);
	max_buttons_width(term, dlg->items + 11, 2, &max);
	min_buttons_width(term, dlg->items + 11, 2, &min);
	w = dlg->win->term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > dlg->win->term->x - 2 * DIALOG_LB) w = dlg->win->term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;
	rw = 0;
	dlg_format_text(NULL, term, net_msg[0], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += 2;
	dlg_format_text(NULL, term, net_msg[1], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += 2;
	dlg_format_group(NULL, term, net_msg + 2, dlg->items + 2, 9, 0, &y, w, &rw);
	y++;
	dlg_format_buttons(NULL, term, dlg->items + 11, 2, 0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term, net_msg[0], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, &dlg->items[0], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term, net_msg[1], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, &dlg->items[1], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_group(term, term, net_msg + 2, &dlg->items[2], 9, dlg->x + DIALOG_LB, &y, w, NULL);
	y++;
	dlg_format_buttons(term, term, &dlg->items[11], 2, dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

void
net_options(struct terminal *term, void *xxx, void *yyy)
{
	struct dialog *d;

	snprint(max_c_str, 3, max_connections);
	snprint(max_cth_str, 2, max_connections_to_host);
	snprint(max_t_str, 2, max_tries);
	snprint(time_str, 5, receive_timeout);
	snprint(unrtime_str, 5, unrestartable_receive_timeout);

	d = mem_alloc(sizeof(struct dialog) + 14 * sizeof(struct dialog_item));
	if (!d) return;
	memset(d, 0, sizeof(struct dialog) + 14 * sizeof(struct dialog_item));

	d->title = TEXT(T_NETWORK_OPTIONS);
	d->fn = netopt_fn;
	d->refresh = (void (*)(void *))refresh_net;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = http_proxy;

	d->items[1].type = D_FIELD;
	d->items[1].dlen = MAX_STR_LEN;
	d->items[1].data = ftp_proxy;

	d->items[2].type = D_FIELD;
	d->items[2].data = max_c_str;
	d->items[2].dlen = 3;
	d->items[2].fn = check_number;
	d->items[2].gid = 1;
	d->items[2].gnum = 16;

	d->items[3].type = D_FIELD;
	d->items[3].data = max_cth_str;
	d->items[3].dlen = 2;
	d->items[3].fn = check_number;
	d->items[3].gid = 1;
	d->items[3].gnum = 8;

	d->items[4].type = D_FIELD;
	d->items[4].data = max_t_str;
	d->items[4].dlen = 2;
	d->items[4].fn = check_number;
	d->items[4].gid = 1;
	d->items[4].gnum = 16;

	d->items[5].type = D_FIELD;
	d->items[5].data = time_str;
	d->items[5].dlen = 5;
	d->items[5].fn = check_number;
	d->items[5].gid = 1;
	d->items[5].gnum = 1800;

	d->items[6].type = D_FIELD;
	d->items[6].data = unrtime_str;
	d->items[6].dlen = 5;
	d->items[6].fn = check_number;
	d->items[6].gid = 1;
	d->items[6].gnum = 1800;

	d->items[7].type = D_CHECKBOX;
	d->items[7].data = (unsigned char *) &async_lookup;
	d->items[7].dlen = sizeof(int);

	d->items[8].type = D_CHECKBOX;
	d->items[8].data = (unsigned char *) &download_utime;
	d->items[8].dlen = sizeof(int);

	d->items[9].type = D_BUTTON;
	d->items[9].gid = 0;
	d->items[9].fn = dlg_http_options;
	d->items[9].text = TEXT(T_HTTP_OPTIONS);
	d->items[9].data = (unsigned char *) &http_bugs;
	d->items[9].dlen = sizeof(struct http_bugs);

	d->items[10].type = D_BUTTON;
	d->items[10].gid = 0;
	d->items[10].fn = dlg_ftp_options;
	d->items[10].text = TEXT(T_FTP_OPTIONS);
	d->items[10].data = default_anon_pass;
	d->items[10].dlen = MAX_STR_LEN;

	d->items[11].type = D_BUTTON;
	d->items[11].gid = B_ENTER;
	d->items[11].fn = ok_dialog;
	d->items[11].text = TEXT(T_OK);
	d->items[12].type = D_BUTTON;
	d->items[12].gid = B_ESC;

	d->items[12].fn = cancel_dialog;
	d->items[12].text = TEXT(T_CANCEL);

	d->items[13].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}


unsigned char *prg_msg[] = {
	TEXT(T_MAILTO_PROG),
	TEXT(T_TELNET_PROG),
	TEXT(T_TN3270_PROG),
	"",
	NULL	
};

void
netprog_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;

	max_text_width(term, prg_msg[0], &max);
	min_text_width(term, prg_msg[0], &min);
	max_text_width(term, prg_msg[1], &max);
	min_text_width(term, prg_msg[1], &min);
	max_text_width(term, prg_msg[2], &max);
	min_text_width(term, prg_msg[2], &min);
	max_buttons_width(term, dlg->items + 3, 2, &max);
	min_buttons_width(term, dlg->items + 3, 2, &min);
	w = dlg->win->term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > dlg->win->term->x - 2 * DIALOG_LB) w = dlg->win->term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;
	rw = 0;
	dlg_format_text(NULL, term, prg_msg[0], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += 2;
	dlg_format_text(NULL, term, prg_msg[1], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += 2;
	dlg_format_text(NULL, term, prg_msg[2], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += 2;
	dlg_format_buttons(NULL, term, dlg->items + 3, 2, 0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term, prg_msg[0], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, &dlg->items[0], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term, prg_msg[1], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, &dlg->items[1], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term, prg_msg[2], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, &dlg->items[2], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg->items[3], 2, dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

void
net_programs(struct terminal *term, void *xxx, void *yyy)
{
	struct dialog *d;

	d = mem_alloc(sizeof(struct dialog) + 6 * sizeof(struct dialog_item));
	if (!d) return;
	memset(d, 0, sizeof(struct dialog) + 6 * sizeof(struct dialog_item));

	d->title = TEXT(T_MAIL_AND_TELNET_PROGRAMS);
	d->fn = netprog_fn;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = get_prog(&mailto_prog);

	d->items[1].type = D_FIELD;
	d->items[1].dlen = MAX_STR_LEN;
	d->items[1].data = get_prog(&telnet_prog);

	d->items[2].type = D_FIELD;
	d->items[2].dlen = MAX_STR_LEN;
	d->items[2].data = get_prog(&tn3270_prog);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ENTER;
	d->items[3].fn = ok_dialog;
	d->items[3].text = TEXT(T_OK);

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ESC;
	d->items[4].fn = cancel_dialog;
	d->items[4].text = TEXT(T_CANCEL);

	d->items[5].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}

#if 0
void
net_opt_ask(struct terminal *term, void *xxx, void *yyy)
{
	if (list_empty(downloads)) {
		net_options(term, xxx, yyy);
		return;
	}

	msg_box(term, NULL,
		_("Network options"), AL_CENTER,
		_("Warning: configuring network will terminate all running downloads. Do you really want to configure network?"),
		term, 2,
		_("Yes"), (void (*)(void *)) net_options, B_ENTER,
		_("No"), NULL, B_ESC);
}
#endif


unsigned char mc_str[8];
unsigned char doc_str[4];

void
cache_refresh(void *xxx)
{
	memory_cache_size = atoi(mc_str) * 1024;
	max_format_cache_entries = atoi(doc_str);
	count_format_cache();
	shrink_memory(0);
}

unsigned char *cache_texts[] = {
	TEXT(T_MEMORY_CACHE_SIZE__KB),
	TEXT(T_NUMBER_OF_FORMATTED_DOCUMENTS),
	NULL
};

void
cache_opt(struct terminal *term, void *xxx, void *yyy)
{
	struct dialog *d;

	snprint(mc_str, 8, memory_cache_size / 1024);
	snprint(doc_str, 4, max_format_cache_entries);

	d = mem_alloc(sizeof(struct dialog) + 5 * sizeof(struct dialog_item));
	if (!d) return;
	memset(d, 0, sizeof(struct dialog) + 5 * sizeof(struct dialog_item));

	d->title = TEXT(T_CACHE_OPTIONS);
	d->fn = group_fn;
	d->udata = cache_texts;
	d->refresh = (void (*)(void *))cache_refresh;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = 8;
	d->items[0].data = mc_str;
	d->items[0].fn = check_number;
	d->items[0].gid = 0;
	d->items[0].gnum = MAXINT;

	d->items[1].type = D_FIELD;
	d->items[1].dlen = 4;
	d->items[1].data = doc_str;
	d->items[1].fn = check_number;
	d->items[1].gid = 0;
	d->items[1].gnum = 256;

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = ok_dialog;
	d->items[2].text = TEXT(T_OK);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].fn = cancel_dialog;
	d->items[3].text = TEXT(T_CANCEL);

	d->items[4].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}


void
menu_save_html_options(struct terminal *term, void *xxx, struct session *ses)
{
	memcpy(&dds, &ses->ds, sizeof(struct document_setup));
	write_html_config(term);
}

unsigned char marg_str[2];

void
html_refresh(struct session *ses)
{
	ses->ds.margin = atoi(marg_str);
	html_interpret(ses);
	draw_formatted(ses);
	load_frames(ses, ses->screen);
	process_file_requests(ses);
	print_screen_status(ses);
}

unsigned char *html_texts[] = {
	TEXT(T_DISPLAY_TABLES),
	TEXT(T_DISPLAY_FRAMES),
	TEXT(T_DISPLAY_LINKS_TO_IMAGES),
	TEXT(T_LINK_ORDER_BY_COLUMNS),
	TEXT(T_NUMBERED_LINKS),
	TEXT(T_TEXT_MARGIN),
	"",
	TEXT(T_IGNORE_CHARSET_INFO_SENT_BY_SERVER),
	TEXT(T_USE_DOCUMENT_COLOURS),
	TEXT(T_AVOID_DARK_ON_BLACK)
};

int
dlg_assume_cp(struct dialog_data *dlg, struct dialog_item_data *di)
{
	charset_sel_list(dlg->win->term, dlg->dlg->udata2, (int *)di->cdata);
	return 0;
}

void
menu_html_options(struct terminal *term, void *xxx, struct session *ses)
{
	struct dialog *d;

	snprint(marg_str, 2, ses->ds.margin);

	d = mem_alloc(sizeof(struct dialog) + 13 * sizeof(struct dialog_item));
	if (!d) return;
	memset(d, 0, sizeof(struct dialog) + 13 * sizeof(struct dialog_item));

	d->title = TEXT(T_HTML_OPTIONS);
	d->fn = group_fn;
	d->udata = html_texts;
	d->udata2 = ses;
	d->refresh = (void (*)(void *))html_refresh;
	d->refresh_data = ses;

	d->items[0].type = D_CHECKBOX;
	d->items[0].data = (unsigned char *) &ses->ds.tables;
	d->items[0].dlen = sizeof(int);

	d->items[1].type = D_CHECKBOX;
	d->items[1].data = (unsigned char *) &ses->ds.frames;
	d->items[1].dlen = sizeof(int);

	d->items[2].type = D_CHECKBOX;
	d->items[2].data = (unsigned char *) &ses->ds.images;
	d->items[2].dlen = sizeof(int);

	d->items[3].type = D_CHECKBOX;
	d->items[3].data = (unsigned char *) &ses->ds.table_order;
	d->items[3].dlen = sizeof(int);

	d->items[4].type = D_CHECKBOX;
	d->items[4].data = (unsigned char *) &ses->ds.num_links;
	d->items[4].dlen = sizeof(int);

	d->items[5].type = D_FIELD;
	d->items[5].dlen = 2;
	d->items[5].data = marg_str;
	d->items[5].fn = check_number;
	d->items[5].gid = 0;
	d->items[5].gnum = 9;

	d->items[6].type = D_BUTTON;
	d->items[6].gid = 0;
	d->items[6].fn = dlg_assume_cp;
	d->items[6].text = TEXT(T_DEFAULT_CODEPAGE);
	d->items[6].data = (unsigned char *) &ses->ds.assume_cp;
	d->items[6].dlen = sizeof(int);

	d->items[7].type = D_CHECKBOX;
	d->items[7].data = (unsigned char *) &ses->ds.hard_assume;
	d->items[7].dlen = sizeof(int);

	d->items[8].type = D_CHECKBOX;
	d->items[8].data = (unsigned char *) &ses->ds.use_document_colours;
	d->items[8].dlen = sizeof(int);

	d->items[9].type = D_CHECKBOX;
	d->items[9].data = (unsigned char *) &ses->ds.avoid_dark_on_black;
	d->items[9].dlen = sizeof(int);

	d->items[10].type = D_BUTTON;
	d->items[10].gid = B_ENTER;
	d->items[10].fn = ok_dialog;
	d->items[10].text = TEXT(T_OK);

	d->items[11].type = D_BUTTON;
	d->items[11].gid = B_ESC;
	d->items[11].fn = cancel_dialog;
	d->items[11].text = TEXT(T_CANCEL);

	d->items[12].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}


void
menu_set_language(struct terminal *term, void *pcp, struct session *ses)
{
	set_language((int)pcp);
	cls_redraw_all_terminals();
}

void
menu_language_list(struct terminal *term, void *xxx, struct session *ses)
{
	int i, sel;
	unsigned char *n;
	struct menu_item *mi;
	if (!(mi = new_menu(1))) return;
	for (i = 0; i < n_languages(); i++) {
		n = language_name(i);
		add_to_menu(&mi, n, "", "", MENU_FUNC menu_set_language, (void *)i, 0);
	}
	sel = current_language;
	do_menu_selected(term, mi, ses, sel);
}


unsigned char *resize_texts[] = {
	TEXT(T_COLUMNS),
	TEXT(T_ROWS)
};

unsigned char x_str[4];
unsigned char y_str[4];

void
do_resize_terminal(struct terminal *term)
{
	unsigned char str[8];
	strcpy(str, x_str);
	strcat(str, ",");
	strcat(str, y_str);
	do_terminal_function(term, TERM_FN_RESIZE, str);
}

void
dlg_resize_terminal(struct terminal *term, void *xxx, struct session *ses)
{
	struct dialog *d;
	int x = term->x > 999 ? 999 : term->x;
	int y = term->y > 999 ? 999 : term->y;

	sprintf(x_str, "%d", x);
	sprintf(y_str, "%d", y);

	d = mem_alloc(sizeof(struct dialog) + 5 * sizeof(struct dialog_item));
	if (!d) return;
	memset(d, 0, sizeof(struct dialog) + 5 * sizeof(struct dialog_item));

	d->title = TEXT(T_RESIZE_TERMINAL);
	d->fn = group_fn;
	d->udata = resize_texts;
	d->refresh = (void (*)(void *))do_resize_terminal;
	d->refresh_data = term;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = 4;
	d->items[0].data = x_str;
	d->items[0].fn = check_number;
	d->items[0].gid = 1;
	d->items[0].gnum = 999;

	d->items[1].type = D_FIELD;
	d->items[1].dlen = 4;
	d->items[1].data = y_str;
	d->items[1].fn = check_number;
	d->items[1].gid = 1;
	d->items[1].gnum = 999;

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = ok_dialog;
	d->items[2].text = TEXT(T_OK);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].fn = cancel_dialog;
	d->items[3].text = TEXT(T_CANCEL);

	d->items[4].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}
