/* Forms viewing/manipulation handling */
/* $Id: form.c,v 1.17 2003/07/29 09:48:25 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif

#include "elinks.h"

#include "bfu/msgbox.h"
#include "config/kbdbind.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "intl/gettext/libintl.h"
#include "osdep/os_dep.h" /* ASCII_* */
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/textarea.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


/* TODO: Some of these (particulary those encoding routines) would feel better
 * in viewer/common/. --pasky */

/* FIXME: Add comments!! --Zas */


void
fixup_select_state(struct form_control *fc, struct form_state *fs)
{
	register int i = 0;

	assert(fc && fs);
	if_assert_failed return;

	if (fs->state >= 0
	    && fs->state < fc->nvalues
	    && !strcmp(fc->values[fs->state], fs->value))
		return;

	while (i < fc->nvalues) {
		if (!strcmp(fc->values[i], fs->value)) {
			fs->state = i;
			return;
		}
		i++;
	}

	fs->state = 0;

	if (fs->value) mem_free(fs->value);
	fs->value = stracpy(fc->nvalues ? fc->values[0] : (unsigned char *) "");
}

static void
init_ctrl(struct form_control *frm, struct form_state *fs)
{
	assert(frm && fs);
	if_assert_failed return;

	if (fs->value) mem_free(fs->value), fs->value = NULL;

	switch (frm->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_TEXTAREA:
			fs->value = stracpy(frm->default_value);
			fs->state = strlen(frm->default_value);
			fs->vpos = 0;
			break;
		case FC_FILE:
			fs->value = stracpy("");
			fs->state = 0;
			fs->vpos = 0;
			break;
		case FC_CHECKBOX:
		case FC_RADIO:
			fs->state = frm->default_state;
			break;
		case FC_SELECT:
			fs->value = stracpy(frm->default_value);
			fs->state = frm->default_state;
			fixup_select_state(frm, fs);
			break;
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_HIDDEN:
			/* Silence compiler warnings. */
			break;
		default:
			internal("unknown form field type");
	}
}

struct form_state *
find_form_state(struct document_view *f, struct form_control *frm)
{
	struct view_state *vs;
	struct form_state *fs;
	int n;

	assert(f && f->vs && frm);
	if_assert_failed return NULL;

	vs = f->vs;
	n = frm->g_ctrl_num;

	if (n < vs->form_info_len) fs = &vs->form_info[n];
	else {
		fs = mem_realloc(vs->form_info, (n + 1) * sizeof(struct form_state));
		if (!fs) return NULL;
		vs->form_info = fs;
		memset(fs + vs->form_info_len, 0,
		       (n + 1 - vs->form_info_len) * sizeof(struct form_state));
		vs->form_info_len = n + 1;
		fs = &vs->form_info[n];
	}

	if (fs->form_num == frm->form_num
	    && fs->ctrl_num == frm->ctrl_num
	    && fs->g_ctrl_num == frm->g_ctrl_num
	    && fs->position == frm->position
	    && fs->type == frm->type)
		return fs;

	if (fs->value) mem_free(fs->value);
	memset(fs, 0, sizeof(struct form_state));
	fs->form_num = frm->form_num;
	fs->ctrl_num = frm->ctrl_num;
	fs->g_ctrl_num = frm->g_ctrl_num;
	fs->position = frm->position;
	fs->type = frm->type;
	init_ctrl(frm, fs);

	return fs;
}

void
draw_form_entry(struct terminal *t, struct document_view *f, struct link *l)
{
	struct form_state *fs;
	struct form_control *frm;
	struct view_state *vs;
	int xp, yp;
	int xw, yw;
	int vx, vy;

	assert(t && f && f->document && f->vs && l);
	if_assert_failed return;
	frm = l->form;
	assertm(frm, "link %d has no form", (int)(l - f->document->links));
	if_assert_failed return;

	fs = find_form_state(f, frm);
	if (!fs) return;

	xp = f->xp;
	yp = f->yp;
	xw = f->xw;
	yw = f->yw;
	vs = f->vs;
	vx = vs->view_posx;
	vy = vs->view_pos;

	switch (frm->type) {
		unsigned char *s;
		int sl;
		register int i, x, y;

		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
			if (fs->state >= fs->vpos + frm->size)
				fs->vpos = fs->state - frm->size + 1;
			if (fs->state < fs->vpos)
				fs->vpos = fs->state;
			if (!l->n) break;

			y = l->pos[0].y + yp - vy;
			if (y >= yp && y < yp + yw) {
				int len = strlen(fs->value) - fs->vpos;

				x = l->pos[0].x + xp - vx;
				for (i = 0; i < frm->size; i++, x++) {
					if (x >= xp && x < xp + xw) {
						if (fs->value &&
						    i >= -fs->vpos && i < len)
							set_only_char(t, x, y,
								      frm->type != FC_PASSWORD
								      ? fs->value[i + fs->vpos]
								      : '*');
						else
							set_only_char(t, x, y, '_');
					}
				}
			}
			break;
		case FC_TEXTAREA:
			draw_textarea(t, fs, f, l);
			break;
		case FC_CHECKBOX:
		case FC_RADIO:
			if (l->n < 2) break;
			x = l->pos[1].x + xp - vx;
			y = l->pos[1].y + yp - vy;
			if (x >= xp && y >= yp && x < xp + xw && y < yp + yw)
				set_only_char(t, x, y, fs->state ? 'X' : ' ');
			break;
		case FC_SELECT:
			fixup_select_state(frm, fs);
			if (fs->state < frm->nvalues)
				s = frm->labels[fs->state];
			else
				/* XXX: when can this happen? --pasky */
				s = "";
			sl = s ? strlen(s) : 0;
			for (i = 0; i < l->n; i++) {
				x = l->pos[i].x + xp - vx;
				y = l->pos[i].y + yp - vy;
				if (x >= xp && y >= yp && x < xp + xw && y < yp + yw)
					set_only_char(t, x, y, i < sl ? s[i] : '_');
			}
			break;
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_HIDDEN:
			break;
		default:
			internal("Unknown form field type.");
	}
}

void
draw_forms(struct terminal *t, struct document_view *f)
{
	struct link *l1, *l2;

	assert(t && f);
	if_assert_failed return;

	l1 = get_first_link(f);
	l2 = get_last_link(f);

	if (!l1 || !l2) {
		assertm(!l1 && !l2, "get_first_link == %p, get_last_link == %p", l1, l2);
		/* Return path :-). */
		return;
	}
	do {
		if (l1->type != L_LINK)
			draw_form_entry(t, f, l1);
	} while (l1++ < l2);
}


int
has_form_submit(struct document *f, struct form_control *frm)
{
	struct form_control *i;
	int q = 0;

	assert(f && frm);
	if_assert_failed return 0;

	foreach (i, f->forms) if (i->form_num == frm->form_num) {
		if ((i->type == FC_SUBMIT || i->type == FC_IMAGE)) return 1;
		q = 1;
	}
	assertm(q, "form is not on list");
	/* Return path :-). */
	return 0;
}


static inline void
free_succesful_controls(struct list_head *submit)
{
	struct submitted_value *v;

	assert(submit);
	if_assert_failed return;

	foreach (v, *submit) {
		if (v->name) mem_free(v->name);
		if (v->value) mem_free(v->value);
		if (v->file_content) mem_free(v->file_content);
	}
	free_list(*submit);
}

static void
get_succesful_controls(struct document_view *f, struct form_control *fc,
		       struct list_head *subm)
{
	struct form_control *frm;
	int ch;

	assert(f && f->document && fc && subm);
	if_assert_failed return;

	init_list(*subm);
	foreach (frm, f->document->forms) {
		if (frm->form_num == fc->form_num
		    && ((frm->type != FC_SUBMIT &&
			 frm->type != FC_IMAGE &&
			 frm->type != FC_RESET) || frm == fc)
		    && frm->name && frm->name[0]) {
			struct submitted_value *sub;
			int fi = 0;
			struct form_state *fs = find_form_state(f, frm);

			if (!fs) continue;
			if ((frm->type == FC_CHECKBOX
			     || frm->type == FC_RADIO)
			    && !fs->state)
				continue;
			if (frm->type == FC_SELECT && !frm->nvalues)
				continue;
fi_rep:
			sub = mem_calloc(1, sizeof(struct submitted_value));
			if (!sub) continue;

			sub->type = frm->type;
			sub->name = stracpy(frm->name);

			switch (frm->type) {
				case FC_TEXT:
				case FC_PASSWORD:
				case FC_FILE:
				case FC_TEXTAREA:
					sub->value = stracpy(fs->value);
					break;
				case FC_CHECKBOX:
				case FC_RADIO:
				case FC_SUBMIT:
				case FC_HIDDEN:
					sub->value = stracpy(frm->default_value);
					break;
				case FC_SELECT:
					fixup_select_state(frm, fs);
					sub->value = stracpy(fs->value);
					break;
				case FC_IMAGE:
					add_to_strn(&sub->name, fi ? ".x" : ".y");
					sub->value = stracpy("0");
					break;
				default:
					internal("bad form control type");
					mem_free(sub);
					continue;
			}

			sub->frm = frm;
			sub->position = frm->form_num + frm->ctrl_num;

			add_to_list(*subm, sub);

			if (frm->type == FC_IMAGE && !fi) {
				fi = 1;
				goto fi_rep;
			}
		}
	}

	do {
		struct submitted_value *sub, *nx;

		ch = 0;
		foreach (sub, *subm) if (sub->next != (void *)subm)
			if (sub->next->position < sub->position) {
				nx = sub->next;
				del_from_list(sub);
				add_at_pos(nx, sub);
				sub = nx;
				ch = 1;
			}
		foreachback (sub, *subm) if (sub->next != (void *)subm)
			if (sub->next->position < sub->position) {
				nx = sub->next;
				del_from_list(sub);
				add_at_pos(nx, sub);
				sub = nx;
				ch = 1;
			}
	} while (ch);

}

static inline unsigned char *
strip_file_name(unsigned char *f)
{
	unsigned char *n, *l;

	assert(f);
	if_assert_failed return NULL;

	l = f - 1;
	for (n = f; *n; n++) if (dir_sep(*n)) l = n;
	return l + 1;
}

static void
encode_controls(struct list_head *l, struct string *data,
		int cp_from, int cp_to)
{
	struct submitted_value *sv;
	struct conv_table *convert_table = NULL;
	int lst = 0;

	assert(l && data);
	if_assert_failed return;

	if (!init_string(data)) return;

	foreach (sv, *l) {
		unsigned char *p2 = NULL;
		struct document_options o;

		memset(&o, 0, sizeof(o));
		o.plain = 1;
		d_opt = &o;

		if (lst)
			add_char_to_string(data, '&');
		else
			lst = 1;

		encode_uri_string(data, sv->name);
		add_char_to_string(data, '=');

		/* Convert back to original encoding (see html_form_control()
		 * for the original recoding). */
		if (sv->type == FC_TEXTAREA) {
			unsigned char *p;

			p = encode_textarea(sv);
			if (p) {
				if (!convert_table)
					convert_table = get_translation_table(cp_from, cp_to);

				p2 = convert_string(convert_table, p,
						    strlen(p));
				mem_free(p);
			}
		} else if (sv->type == FC_TEXT ||
			   sv->type == FC_PASSWORD) {
			if (!convert_table)
				convert_table = get_translation_table(cp_from, cp_to);

			p2 = convert_string(convert_table, sv->value,
					    strlen(sv->value));
		} else {
			p2 = stracpy(sv->value);
		}

		if (p2) {
			encode_uri_string(data, p2);
			mem_free(p2);
		}
	}
}



#define BL	32

/* FIXME: shouldn't we encode data at send time (in http.c) ? --Zas */
static void
encode_multipart(struct session *ses, struct list_head *l, struct string *data,
		 unsigned char *bound, int cp_from, int cp_to)
{
	struct conv_table *convert_table = NULL;
	struct submitted_value *sv;
	int *nbp, *bound_ptrs = NULL;
	int nbound_ptrs = 0;
	int flg = 0;
	register int i;

	assert(ses && l && data && bound);
	if_assert_failed return;

	if (!init_string(data)) return;

	memset(bound, 'x', BL);

	foreach (sv, *l) {

bnd:
		add_to_string(data, "--");
		if (!(nbound_ptrs & (ALLOC_GR-1))) {
			nbp = mem_realloc(bound_ptrs, (nbound_ptrs + ALLOC_GR) * sizeof(int));
			if (!nbp) goto xx;
			bound_ptrs = nbp;
		}
		bound_ptrs[nbound_ptrs++] = data->length;

xx:
		add_bytes_to_string(data, bound, BL);
		if (flg) break;
		add_to_string(data, "\r\nContent-Disposition: form-data; name=\"");
		add_to_string(data, sv->name);
		if (sv->type == FC_FILE) {
#define F_BUFLEN 1024
			int fh, rd;
			unsigned char buffer[F_BUFLEN];

			add_to_string(data, "\"; filename=\"");
			add_to_string(data, strip_file_name(sv->value));
			/* It sends bad data if the file name contains ", but
			   Netscape does the same */
			/* FIXME: is this a reason ? --Zas */
			add_to_string(data, "\"\r\n\r\n");

			if (*sv->value) {
				if (get_opt_int_tree(cmdline_options, "anonymous"))
					goto encode_error;

				/* FIXME: DO NOT COPY FILE IN MEMORY !! --Zas */
				fh = open(sv->value, O_RDONLY);
				if (fh == -1) goto encode_error;
				set_bin(fh);
				do {
					rd = read(fh, buffer, F_BUFLEN);
					if (rd == -1) goto encode_error;
					if (rd) add_bytes_to_string(data, buffer, rd);
				} while (rd);
				close(fh);
			}
#undef F_BUFLEN
		} else {
			struct document_options o;

			add_to_string(data, "\"\r\n\r\n");

			memset(&o, 0, sizeof(o));
			o.plain = 1;
			d_opt = &o;

			/* Convert back to original encoding (see
			 * html_form_control() for the original recoding). */
			if (sv->type == FC_TEXT || sv->type == FC_PASSWORD ||
			    sv->type == FC_TEXTAREA) {
				unsigned char *p;

				if (!convert_table)
				       	convert_table = get_translation_table(cp_from,
									      cp_to);

				p = convert_string(convert_table, sv->value,
						   strlen(sv->value));
				if (p) {
					add_to_string(data, p);
					mem_free(p);
				}
			} else {
				add_to_string(data, sv->value);
			}
		}

		add_to_string(data, "\r\n");
	}

	if (!flg) {
		flg = 1;
		goto bnd;
	}

	add_to_string(data, "--\r\n");
	memset(bound, '0', BL);

again:
	for (i = 0; i <= data->length - BL; i++) {
		int j;

		for (j = 0; j < BL; j++) if ((data->source)[i + j] != bound[j]) goto nb;
		for (j = BL - 1; j >= 0; j--)
			if (bound[j]++ >= '9') bound[j] = '0';
			else goto again;
		internal("Could not assing boundary");

nb:;
	}

	for (i = 0; i < nbound_ptrs; i++)
		memcpy(data->source + bound_ptrs[i], bound, BL);

	mem_free(bound_ptrs);
	return;

encode_error:
	mem_free(bound_ptrs);
	done_string(data);

	{
	unsigned char *m1, *m2;

	/* XXX: This error message should move elsewhere. --Zas */
	m1 = stracpy(sv->value);
	if (!m1) return;
	m2 = stracpy((unsigned char *) strerror(errno));
	msg_box(ses->tab->term, getml(m1, m2, NULL), MSGBOX_FREE_TEXT,
		N_("Error while posting form"), AL_CENTER,
		msg_text(ses->tab->term, N_("Could not get file %s: %s"),
			 m1, m2),
		ses, 1,
		N_("Cancel"), NULL, B_ENTER | B_ESC);
	}
}

static void
reset_form(struct document_view *f, int form_num)
{
	struct form_control *frm;

	assert(f && f->document);
	if_assert_failed return;

	foreach (frm, f->document->forms) if (frm->form_num == form_num) {
		struct form_state *fs = find_form_state(f, frm);

		if (fs) init_ctrl(frm, fs);
	}
}

unsigned char *
get_form_url(struct session *ses, struct document_view *f,
	     struct form_control *frm)
{
	struct list_head submit;
	struct string data;
	struct string go;
	unsigned char bound[BL];
	int cp_from, cp_to;

	assert(ses && ses->tab && ses->tab->term);
	if_assert_failed return NULL;
	assert(f && f->document && frm);
	if_assert_failed return NULL;

	if (frm->type == FC_RESET) {
		reset_form(f, frm->form_num);
		return NULL;
	}
	if (!frm->action) return NULL;

	get_succesful_controls(f, frm, &submit);

	cp_from = get_opt_int_tree(ses->tab->term->spec, "charset");
	cp_to = f->document->cp;
	if (frm->method == FM_GET || frm->method == FM_POST)
		encode_controls(&submit, &data, cp_from, cp_to);
	else
		encode_multipart(ses, &submit, &data, bound, cp_from, cp_to);

	if (!data.source) {
		free_succesful_controls(&submit);
		return NULL;
	}

	if (!init_string(&go)) return NULL;

	if (frm->method == FM_GET) {
		unsigned char *pos = strchr(frm->action, '#');

		if (pos) {
			add_bytes_to_string(&go, frm->action, pos - frm->action);
		} else {
			add_to_string(&go, frm->action);
		}

		if (strchr(go.source, '?'))
			add_char_to_string(&go, '&');
		else
			add_char_to_string(&go, '?');

		add_string_to_string(&go, &data);

		if (pos) add_to_string(&go, pos);
	} else {
		register int i;

		add_to_string(&go, frm->action);
		add_char_to_string(&go, POST_CHAR);
		if (frm->method == FM_POST) {
			add_to_string(&go, "application/x-www-form-urlencoded\n");
		} else {
			add_to_string(&go, "multipart/form-data; boundary=");
			add_bytes_to_string(&go, bound, BL);
			add_char_to_string(&go, '\n');
		}
		for (i = 0; i < data.length; i++) {
			unsigned char p[3];

			ulonghexcat(p, NULL, (int) data.source[i], 2, '0', 0);
			add_to_string(&go, p);
		}
	}

	done_string(&data);
	free_succesful_controls(&submit);
	return go.source;
}

#undef BL


/* This is common backend for submit_form() and submit_form_reload(). */
static int
submit_form_do(struct terminal *term, void *xxx, struct session *ses,
	       int do_reload)
{
	struct document_view *fd;
	struct link *link;

	assert(term && ses);
	if_assert_failed return 1;
	fd = current_frame(ses);

	assert(fd && fd->vs && fd->document);
	if_assert_failed return 1;
	if (fd->vs->current_link == -1) return 1;
	link = &fd->document->links[fd->vs->current_link];

	return goto_link(get_form_url(ses, fd, link->form), link->target, ses, do_reload);
}

int
submit_form(struct terminal *term, void *xxx, struct session *ses)
{
	assert(term && ses);
	if_assert_failed return 1;
	return submit_form_do(term, xxx, ses, 0);
}

int
submit_form_reload(struct terminal *term, void *xxx, struct session *ses)
{
	assert(term && ses);
	if_assert_failed return 1;
	return submit_form_do(term, xxx, ses, 1);
}


int
field_op(struct session *ses, struct document_view *f, struct link *l,
	 struct event *ev, int rep)
{
	struct form_control *frm;
	struct form_state *fs;
	int x = 1;

	assert(ses && f && l && ev);
	if_assert_failed return 0;
	frm = l->form;
	assertm(frm, "link has no form control");
	if_assert_failed return 0;

	if (l->form->ro == 2) return 0;
	fs = find_form_state(f, frm);
	if (!fs || !fs->value) return 0;

	if (ev->ev == EV_KBD) {
		switch (kbd_action(KM_EDIT, ev, NULL)) {
			case ACT_LEFT:
				fs->state = fs->state ? fs->state - 1 : 0;
				break;
			case ACT_RIGHT:
				{
					int fsv_len = strlen(fs->value);

					fs->state = fs->state < fsv_len
						    ? fs->state + 1
						      : fsv_len;
				}
				break;
			case ACT_HOME:
				if (frm->type == FC_TEXTAREA) {
					if (textarea_op_home(fs, frm, rep))
						goto b;
				} else fs->state = 0;
				break;
			case ACT_UP:
				if (frm->type == FC_TEXTAREA) {
					if (textarea_op_up(fs, frm, rep))
						goto b;
				} else x = 0;
				break;
			case ACT_DOWN:
				if (frm->type == FC_TEXTAREA) {
					if (textarea_op_down(fs, frm, rep))
						goto b;
				} else x = 0;
				break;
			case ACT_END:
				if (frm->type == FC_TEXTAREA) {
					if (textarea_op_end(fs, frm, rep))
						goto b;
				} else fs->state = strlen(fs->value);
				break;
			case ACT_EDIT:
				if (frm->type == FC_TEXTAREA && !frm->ro)
				  	textarea_edit(0, ses->tab->term, frm, fs, f, l);
				break;
			case ACT_COPY_CLIPBOARD:
				set_clipboard_text(fs->value);
				break;
			case ACT_CUT_CLIPBOARD:
				set_clipboard_text(fs->value);
				if (!frm->ro) fs->value[0] = 0;
				fs->state = 0;
				break;
			case ACT_PASTE_CLIPBOARD: {
				char *clipboard = get_clipboard_text();

				if (!clipboard) break;
				if (!frm->ro) {
					int cb_len = strlen(clipboard);

					if (cb_len <= frm->maxlength) {
						unsigned char *v = mem_realloc(fs->value, cb_len + 1);

						if (v) {
							fs->value = v;
							memmove(v, clipboard, cb_len + 1);
							fs->state = strlen(fs->value);
						}
					}
				}
				mem_free(clipboard);
				break;
			}
			case ACT_ENTER:
				if (frm->type == FC_TEXTAREA) {
					if (textarea_op_enter(fs, frm, rep))
						goto b;
				} else x = 0;
				break;
			case ACT_BACKSPACE:
				if (!frm->ro && fs->state)
					memmove(fs->value + fs->state - 1, fs->value + fs->state,
						strlen(fs->value + fs->state) + 1),
					fs->state--;
				break;
			case ACT_DELETE:
				if (!frm->ro && fs->state < strlen(fs->value))
					memmove(fs->value + fs->state, fs->value + fs->state + 1,
						strlen(fs->value + fs->state));
				break;
			case ACT_KILL_TO_BOL:
				if (!frm->ro && fs->state > 0) {
					unsigned char *prev;

					/* TODO: Make this memrchr(), and
					 * introduce stub for that function
					 * into util/string.*. --pasky */
					for (prev = fs->value + fs->state - 1;
					     prev > fs->value
						&& *prev != ASCII_LF;
					     prev--)
						;

					if (prev > fs->value
					    && fs->value[fs->state - 1]
						    != ASCII_LF)
						prev++;

					memmove(prev,
						fs->value + fs->state,
						strlen(fs->value + fs->state)
						 + 1);

					fs->state = (int) (prev - fs->value);
				}
				break;
		    	case ACT_KILL_TO_EOL:
				if (!frm->ro && fs->value[fs->state]) {
					unsigned char *rest;

					rest = strchr(fs->value + fs->state,
						      ASCII_LF);

					if (!rest) {
						fs->value[fs->state] = '\0';
						break;
					}

					if (fs->value[fs->state] == ASCII_LF)
						++rest;

					memmove(fs->value + fs->state, rest,
						strlen(rest) + 1);
				}
 				break;
			default:
				if (!ev->y && (ev->x >= 32 && ev->x < 256)) {
					int value_len = strlen(fs->value);

					if (!frm->ro && value_len < frm->maxlength) {
						unsigned char *v = mem_realloc(fs->value, value_len + 2);

						if (v) {
							fs->value = v;
							memmove(v + fs->state + 1, v + fs->state, strlen(v + fs->state) + 1);
							v[fs->state++] = ev->x;
						}
					}
				} else {

b:
					x = 0;
				}
		}
	} else x = 0;

	if (x) {
		draw_form_entry(ses->tab->term, f, l);
		redraw_from_window(ses->tab);
	}
	return x;
}
