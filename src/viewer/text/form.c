/* Forms viewing/manipulation handling */
/* $Id: form.c,v 1.124 2004/06/09 22:54:23 zas Exp $ */

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

#include "bfu/listmenu.h"
#include "bfu/msgbox.h"
#include "config/kbdbind.h"
#include "document/document.h"
#include "document/html/parser.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "formhist/formhist.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/file.h"
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

static void
fixup_select_state(struct form_control *fc, struct form_state *fs)
{
	register int i;

	assert(fc && fs);
	if_assert_failed return;

	if (fs->state >= 0
	    && fs->state < fc->nvalues
	    && !strcmp(fc->values[fs->state], fs->value))
		return;

	for (i = 0; i < fc->nvalues; i++)
		if (!strcmp(fc->values[i], fs->value)) {
			fs->state = i;
			return;
		}

	fs->state = 0;

	mem_free_set(&fs->value, stracpy(fc->nvalues
					 ? fc->values[0]
					 : (unsigned char *) ""));
}

void
selected_item(struct terminal *term, void *pitem, struct session *ses)
{
	int item = (int) pitem;
	struct document_view *doc_view;
	struct link *link;
	struct form_state *fs;

	assert(term && ses);
	if_assert_failed return;
	doc_view = current_frame(ses);

	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	link = get_current_link(doc_view);
	if (!link || link->type != LINK_SELECT) return;

	fs = find_form_state(doc_view, link->form);
	if (fs) {
		struct form_control *frm = link->form;

		if (item >= 0 && item < frm->nvalues) {
			fs->state = item;
			mem_free_set(&fs->value, stracpy(frm->values[item]));
		}
		fixup_select_state(frm, fs);
	}

	refresh_view(ses, doc_view, 0);
}

static void
init_form_state(struct form_control *frm, struct form_state *fs)
{
	assert(frm && fs);
	if_assert_failed return;

	mem_free_set(&fs->value, NULL);

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
	}
}

void
done_form_control(struct form_control *fc)
{
	int i;

	assert(fc);
	if_assert_failed return;

	mem_free_if(fc->action);
	mem_free_if(fc->target);
	mem_free_if(fc->name);
	mem_free_if(fc->alt);
	mem_free_if(fc->default_value);

	for (i = 0; i < fc->nvalues; i++) {
		mem_free_if(fc->values[i]);
		mem_free_if(fc->labels[i]);
	}

	mem_free_if(fc->values);
	mem_free_if(fc->labels);
	if (fc->menu) free_menu(fc->menu);
}

struct form_state *
find_form_state(struct document_view *doc_view, struct form_control *frm)
{
	struct view_state *vs;
	struct form_state *fs;
	int n;

	assert(doc_view && doc_view->vs && frm);
	if_assert_failed return NULL;

	vs = doc_view->vs;
	n = frm->g_ctrl_num;

	if (n >= vs->form_info_len) {
		int nn = n + 1;

		fs = mem_realloc(vs->form_info, nn * sizeof(struct form_state));
		if (!fs) return NULL;
		memset(fs + vs->form_info_len, 0,
		       (nn - vs->form_info_len) * sizeof(struct form_state));
		vs->form_info = fs;
		vs->form_info_len = nn;
	}
	fs = &vs->form_info[n];

	if (fs->form_num == frm->form_num
	    && fs->ctrl_num == frm->ctrl_num
	    && fs->g_ctrl_num == frm->g_ctrl_num
	    && fs->position == frm->position
	    && fs->type == frm->type)
		return fs;

	mem_free_if(fs->value);
	memset(fs, 0, sizeof(struct form_state));
	fs->form_num = frm->form_num;
	fs->ctrl_num = frm->ctrl_num;
	fs->g_ctrl_num = frm->g_ctrl_num;
	fs->position = frm->position;
	fs->type = frm->type;
	init_form_state(frm, fs);

	return fs;
}

void
draw_form_entry(struct terminal *term, struct document_view *doc_view,
		struct link *link)
{
	struct form_state *fs;
	struct form_control *frm;
	struct view_state *vs;
	struct box *box;
	int dx, dy;

	assert(term && doc_view && doc_view->document && doc_view->vs && link);
	if_assert_failed return;

	frm = link->form;
	assertm(frm, "link %d has no form", (int)(link - doc_view->document->links));
	if_assert_failed return;

	fs = find_form_state(doc_view, frm);
	if (!fs) return;

	box = &doc_view->box;
	vs = doc_view->vs;
	dx = box->x - vs->x;
	dy = box->y - vs->y;
	switch (frm->type) {
		unsigned char *s;
		int len;
		register int i, x, y;

		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
			int_bounds(&fs->vpos, fs->state - frm->size + 1, fs->state);
			if (!link->n) break;

			y = link->pos[0].y + dy;
			if (row_is_in_box(box, y)) {
				len = strlen(fs->value) - fs->vpos;
				x = link->pos[0].x + dx;
				for (i = 0; i < frm->size; i++, x++) {
					if (!col_is_in_box(box, x)) continue;
					if (fs->value && i >= -fs->vpos && i < len)
						draw_char_data(term, x, y,
							       frm->type != FC_PASSWORD
							       ? fs->value[i + fs->vpos]
							       : '*');
					else
						draw_char_data(term, x, y, '_');
				}
			}
			break;
		case FC_TEXTAREA:
			draw_textarea(term, fs, doc_view, link);
			break;
		case FC_CHECKBOX:
		case FC_RADIO:
			if (link->n < 2) break;
			x = link->pos[1].x + dx;
			y = link->pos[1].y + dy;
			if (is_in_box(box, x, y))
				draw_char_data(term, x, y, fs->state ? 'X' : ' ');
			break;
		case FC_SELECT:
			fixup_select_state(frm, fs);
			if (fs->state < frm->nvalues)
				s = frm->labels[fs->state];
			else
				/* XXX: when can this happen? --pasky */
				s = "";
			len = s ? strlen(s) : 0;
			for (i = 0; i < link->n; i++) {
				x = link->pos[i].x + dx;
				y = link->pos[i].y + dy;
				if (is_in_box(box, x, y))
					draw_char_data(term, x, y, i < len ? s[i] : '_');
			}
			break;
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_HIDDEN:
			break;
	}
}

void
draw_forms(struct terminal *t, struct document_view *doc_view)
{
	struct link *l1, *l2;

	assert(t && doc_view);
	if_assert_failed return;

	l1 = get_first_link(doc_view);
	l2 = get_last_link(doc_view);

	if (!l1 || !l2) {
		assertm(!l1 && !l2, "get_first_link == %p, get_last_link == %p", l1, l2);
		/* Return path :-). */
		return;
	}
	do {
		if (!link_is_form(l1)) continue;
#ifdef CONFIG_FORMHIST
		if (l1->form->type == FC_TEXT
		    || l1->form->type == FC_PASSWORD) {
			unsigned char *value;

			value = get_form_history_value(l1->form->action,
						       l1->form->name);

			if (value)
				mem_free_set(&l1->form->default_value, stracpy(value));
		}
#endif /* CONFIG_FORMHIST */
		draw_form_entry(t, doc_view, l1);

	} while (l1++ < l2);
}


int
has_form_submit(struct document *document, struct form_control *frm)
{
	struct form_control *fc;
	int found = 0;

	assert(document && frm);
	if_assert_failed return 0;

	foreach (fc, document->forms) {
		if (fc->form_num != frm->form_num) continue;
		found = 1;
		if (fc->type == FC_SUBMIT || fc->type == FC_IMAGE)
			break;
	}

	assertm(found, "form is not on list");
	/* Return path :-). */
	return found;
}


static inline void
free_succesful_controls(struct list_head *submit)
{
	struct submitted_value *v;

	assert(submit);
	if_assert_failed return;

	foreach (v, *submit) {
		mem_free_if(v->name);
		mem_free_if(v->value);
	}
	free_list(*submit);
}

static void
add_submitted_value_to_list(struct form_control *frm,
		            struct form_state *fs,
		            struct list_head *list)
{
	struct submitted_value *sub;
	int fi = 0;

	assert(frm && fs && list);

	if ((frm->type == FC_CHECKBOX
	     || frm->type == FC_RADIO)
	    && !fs->state)
		return;

	if (frm->type == FC_SELECT && !frm->nvalues)
		return;

fi_rep:
	sub = mem_calloc(1, sizeof(struct submitted_value));
	if (!sub) return;

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
	case FC_RESET:
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
	}

	sub->frm = frm;
	sub->position = frm->form_num + frm->ctrl_num;

	add_to_list(*list, sub);

	if (frm->type == FC_IMAGE && !fi) {
		fi = 1;
		goto fi_rep;
	}
}

static void
sort_submitted_values(struct list_head *list)
{
	int changed;

	do {
		struct submitted_value *sub, *next;

		changed = 0;
		foreach (sub, *list) if (sub->next != (void *)list)
			if (sub->next->position < sub->position) {
				next = sub->next;
				del_from_list(sub);
				add_at_pos(next, sub);
				sub = next;
				changed = 1;
			}
		foreachback (sub, *list) if (sub->next != (void *)list)
			if (sub->next->position < sub->position) {
				next = sub->next;
				del_from_list(sub);
				add_at_pos(next, sub);
				sub = next;
				changed = 1;
			}
	} while (changed);
}

static void
get_succesful_controls(struct document_view *doc_view, struct form_control *fc,
		       struct list_head *list)
{
	struct form_control *frm;

	assert(doc_view && doc_view->document && fc && list);
	if_assert_failed return;

	foreach (frm, doc_view->document->forms) {
		if (frm->form_num == fc->form_num
		    && ((frm->type != FC_SUBMIT &&
			 frm->type != FC_IMAGE &&
			 frm->type != FC_RESET) || frm == fc)
		    && frm->name && frm->name[0]) {
			struct form_state *fs = find_form_state(doc_view, frm);

			if (!fs) continue;

			add_submitted_value_to_list(frm, fs, list);
		}
	}

	sort_submitted_values(list);
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

	foreach (sv, *l) {
		unsigned char *p2 = NULL;

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
						    strlen(p), CSM_FORM, NULL);
				mem_free(p);
			}
		} else if (sv->type == FC_TEXT ||
			   sv->type == FC_PASSWORD) {
			if (!convert_table)
				convert_table = get_translation_table(cp_from, cp_to);

			p2 = convert_string(convert_table, sv->value,
					    strlen(sv->value), CSM_FORM, NULL);
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
#define realloc_bound_ptrs(bptrs, bptrs_size) \
	mem_align_alloc(bptrs, bptrs_size, bptrs_size + 1, int, 0xFF)

static inline void
add_boundary(struct string *data, unsigned char *bound,
	     int **bound_ptrs, int *nbound_ptrs)
{
	add_to_string(data, "--");
	if (realloc_bound_ptrs(bound_ptrs, *nbound_ptrs))
		*bound_ptrs[*nbound_ptrs++] = data->length;

	add_bytes_to_string(data, bound, BL);
}

/* FIXME: shouldn't we encode data at send time (in http.c) ? --Zas */
static void
encode_multipart(struct session *ses, struct list_head *l, struct string *data,
		 unsigned char *bound, int cp_from, int cp_to)
{
	struct conv_table *convert_table = NULL;
	struct submitted_value *sv;
	int *bound_ptrs = NULL;
	int nbound_ptrs = 0;
	register int i;

	assert(ses && l && data && bound);
	if_assert_failed return;

	memset(bound, 'x', BL);

	foreach (sv, *l) {
		add_boundary(data, bound, &bound_ptrs, &nbound_ptrs);

		/* FIXME: name is not encoded.
		 * from RFC 1867:
		 * multipart/form-data contains a series of parts.
		 * Each part is expected to contain a content-disposition
		 * header where the value is "form-data" and a name attribute
		 * specifies the field name within the form,
		 * e.g., 'content-disposition: form-data; name="xxxxx"',
		 * where xxxxx is the field name corresponding to that field.
		 * Field names originally in non-ASCII character sets may be
		 * encoded using the method outlined in RFC 1522. */
		add_to_string(data, "\r\nContent-Disposition: form-data; name=\"");
		add_to_string(data, sv->name);
		if (sv->type == FC_FILE) {
#define F_BUFLEN 1024
			int fh, rd;
			unsigned char buffer[F_BUFLEN];

			add_to_string(data, "\"; filename=\"");
			add_to_string(data, get_filename_position(sv->value));
			/* It sends bad data if the file name contains ", but
			   Netscape does the same */
			/* FIXME: We should follow RFCs 1522, 1867,
			 * 2047 (updated by rfc 2231), to provide correct support
			 * for non-ASCII and special characters in values. --Zas */
			add_to_string(data, "\"\r\n\r\n");

			if (*sv->value) {
				unsigned char *filename;

				if (get_opt_int_tree(cmdline_options, "anonymous"))
					goto encode_error;

				/* FIXME: DO NOT COPY FILE IN MEMORY !! --Zas */
				filename = expand_tilde(sv->value);
				if (!filename) goto encode_error;

				fh = open(filename, O_RDONLY);
				mem_free(filename);

				if (fh == -1) goto encode_error;
				set_bin(fh);
				do {
					rd = safe_read(fh, buffer, F_BUFLEN);
					if (rd == -1) goto encode_error;
					if (rd) add_bytes_to_string(data, buffer, rd);
				} while (rd);
				close(fh);
			}
#undef F_BUFLEN
		} else {
			add_to_string(data, "\"\r\n\r\n");

			/* Convert back to original encoding (see
			 * html_form_control() for the original recoding). */
			if (sv->type == FC_TEXT || sv->type == FC_PASSWORD ||
			    sv->type == FC_TEXTAREA) {
				unsigned char *p;

				if (!convert_table)
					convert_table = get_translation_table(cp_from,
									      cp_to);

				p = convert_string(convert_table, sv->value,
						   strlen(sv->value), CSM_FORM, NULL);
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

	add_boundary(data, bound, &bound_ptrs, &nbound_ptrs);
	add_to_string(data, "--\r\n");
	memset(bound, '0', BL);

again:
	for (i = 0; i <= data->length - BL; i++) {
		register int j;

		for (j = 0; j < BL; j++)
			if ((data->source)[i + j] != bound[j])
				goto next_boundary;

		for (j = BL - 1; j >= 0; j--)
			if (bound[j]++ >= '9')
				bound[j] = '0';
			else
				goto again;

		INTERNAL("Could not assing boundary");

next_boundary:;
	}

	for (i = 0; i < nbound_ptrs; i++)
		memcpy(data->source + bound_ptrs[i], bound, BL);

	mem_free(bound_ptrs);
	return;

encode_error:
	mem_free(bound_ptrs);
	done_string(data);

	/* XXX: This error message should move elsewhere. --Zas */
	msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
		N_("Error while posting form"), AL_CENTER,
		msg_text(ses->tab->term, N_("Could not get file %s: %s"),
			 sv->value, strerror(errno)),
		ses, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

static void
encode_newlines(struct string *string, unsigned char *data)
{
	unsigned char buffer[4];

	memset(buffer, 0, sizeof(buffer));

	for (; *data; data++) {
		if (*data == '\n' || *data == '\r') {
			/* Hex it. */
			buffer[0] = '%';
			buffer[1] = hx((((int) *data) & 0xF0) >> 4);
			buffer[2] = hx(((int) *data) & 0xF);
		} else {
			buffer[0] = *data;
			buffer[1] = 0;
		}

		add_to_string(string, buffer);
	}
}

static void
encode_text_plain(struct list_head *l, struct string *data,
		  int cp_from, int cp_to)
{
	struct submitted_value *sv;
	struct conv_table *convert_table = get_translation_table(cp_from, cp_to);

	assert(l && data);
	if_assert_failed return;

	foreach (sv, *l) {
		unsigned char *area51 = NULL;
		unsigned char *value = sv->value;

		add_to_string(data, sv->name);
		add_char_to_string(data, '=');

		switch (sv->type) {
		case FC_TEXTAREA:
			value = area51 = encode_textarea(sv);
			if (!area51) break;
			/* Fall through */
		case FC_TEXT:
		case FC_PASSWORD:
			/* Convert back to original encoding (see
			 * html_form_control() for the original recoding). */
			value = convert_string(convert_table, value,
					       strlen(value), CSM_FORM, NULL);
		default:
			/* Falling right through to free that textarea stuff */
			mem_free_if(area51);

			/* Did the conversion fail? */
			if (!value) break;

			encode_newlines(data, value);

			/* Free if we did convert something */
			if (value != sv->value) mem_free(value);
		}

		add_to_string(data, "\r\n");
	}
}

static void
do_reset_form(struct document_view *doc_view, int form_num)
{
	struct form_control *frm;

	assert(doc_view && doc_view->document);
	if_assert_failed return;

	foreach (frm, doc_view->document->forms)
		if (frm->form_num == form_num) {
			struct form_state *fs = find_form_state(doc_view, frm);

			if (fs) init_form_state(frm, fs);
		}
}

void
reset_form(struct session *ses, struct document_view *doc_view, int a)
{
	struct link *link = get_current_link(doc_view);

	if (!link) return;

	do_reset_form(doc_view, link->form->form_num);
	draw_forms(ses->tab->term, doc_view);
}

struct uri *
get_form_uri(struct session *ses, struct document_view *doc_view,
	     struct form_control *frm)
{
	INIT_LIST_HEAD(submit);
	struct string data;
	struct string go;
	unsigned char bound[BL];
	int cp_from, cp_to;
	struct uri *uri;

	assert(ses && ses->tab && ses->tab->term);
	if_assert_failed return NULL;
	assert(doc_view && doc_view->document && frm);
	if_assert_failed return NULL;

	if (frm->type == FC_RESET) {
		do_reset_form(doc_view, frm->form_num);
		return NULL;
	}

	if (!frm->action
	    || !init_string(&data))
		return NULL;

	get_succesful_controls(doc_view, frm, &submit);

	cp_from = get_opt_int_tree(ses->tab->term->spec, "charset");
	cp_to = doc_view->document->cp;
	switch (frm->method) {
	case FM_GET:
	case FM_POST:
		encode_controls(&submit, &data, cp_from, cp_to);
		break;

	case FM_POST_MP:
		encode_multipart(ses, &submit, &data, bound, cp_from, cp_to);
		break;

	case FM_POST_TEXT_PLAIN:
		encode_text_plain(&submit, &data, cp_from, cp_to);
	}

#ifdef CONFIG_FORMHIST
	/* XXX: We check data.source here because a NULL value can indicate
	 * not only a memory allocation failure, but also an error reading
	 * a file that is to be uploaded. TODO: Distinguish between
	 * these two classes of errors (is it worth it?). -- Miciah */
	if (data.source
	    && get_opt_bool("document.browse.forms.show_formhist"))
		memorize_form(ses, &submit, frm);
#endif

	free_succesful_controls(&submit);

	if (!data.source
	    || !init_string(&go)) {
		done_string(&data);
		return NULL;
	}

	switch (frm->method) {
	case FM_GET:
	{
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
		break;
	}
	case FM_POST:
	case FM_POST_MP:
	case FM_POST_TEXT_PLAIN:
	{
		register int i;

		add_to_string(&go, frm->action);
		add_char_to_string(&go, POST_CHAR);
		if (frm->method == FM_POST) {
			add_to_string(&go, "application/x-www-form-urlencoded\n");

		} else if (frm->method == FM_POST_TEXT_PLAIN) {
			/* Dunno about this one but we don't want the full
			 * hextcat thingy. --jonas */
			add_to_string(&go, "text/plain\n");
			add_to_string(&go, data.source);
			break;

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
	}

	done_string(&data);

	uri = get_uri(go.source, 0);
	done_string(&go);
	if (uri) uri->form = 1;

	return uri;
}

#undef BL


void
submit_form(struct session *ses, struct document_view *doc_view, int do_reload)
{
	goto_current_link(ses, doc_view, do_reload);
}

void
auto_submit_form(struct session *ses)
{
	struct document *document = ses->doc_view->document;
	struct form_control *form = document->forms.next;
	int link;

	for (link = 0; link < document->nlinks; link++)
		if (document->links[link].form == form)
			break;

	if (link >= document->nlinks) return;

	ses->doc_view->vs->current_link = link;
	submit_form(ses, ses->doc_view, 0);
}

int
field_op_do(struct terminal *term, struct document_view *doc_view,
	    struct form_control *frm, struct form_state *fs, struct link *l,
	    struct term_event *ev, int rep)
{
	int x = 1;

	switch (kbd_action(KM_EDIT, ev, NULL)) {
		case ACT_EDIT_LEFT:
			fs->state = int_max(fs->state - 1, 0);
			break;
		case ACT_EDIT_RIGHT:
			fs->state = int_min(fs->state + 1, strlen(fs->value));
			break;
		case ACT_EDIT_HOME:
			if (frm->type == FC_TEXTAREA) {
				if (textarea_op_home(fs, frm, rep)) {
					x = 0;
					break;
				}
			} else {
				fs->state = 0;
			}
			break;
		case ACT_EDIT_UP:
			if (frm->type == FC_TEXTAREA) {
				if (textarea_op_up(fs, frm, rep)) {
					x = 0;
					break;
				}
			} else {
				x = 0;
			}
			break;
		case ACT_EDIT_DOWN:
			if (frm->type == FC_TEXTAREA) {
				if (textarea_op_down(fs, frm, rep)) {
					x = 0;
					break;
				}
			} else {
				x = 0;
			}
			break;
		case ACT_EDIT_END:
			if (frm->type == FC_TEXTAREA) {
				if (textarea_op_end(fs, frm, rep)) {
					x = 0;
					break;
				}
			} else {
				fs->state = strlen(fs->value);
			}
			break;
		case ACT_EDIT_BEGINNING_OF_BUFFER:
			if (frm->type == FC_TEXTAREA) {
				if (textarea_op_bob(fs, frm, rep)) {
					x = 0;
					break;
				}
			} else {
				fs->state = 0;
			}
			break;
		case ACT_EDIT_END_OF_BUFFER:
			if (frm->type == FC_TEXTAREA) {
				if (textarea_op_eob(fs, frm, rep)) {
					x = 0;
					break;
				}
			} else {
				fs->state = strlen(fs->value);
			}
			break;
		case ACT_EDIT_EDIT:
			if (frm->type == FC_TEXTAREA && !frm->ro)
			  	textarea_edit(0, term, frm, fs, doc_view, l);
			break;
		case ACT_EDIT_COPY_CLIPBOARD:
			set_clipboard_text(fs->value);
			break;
		case ACT_EDIT_CUT_CLIPBOARD:
			set_clipboard_text(fs->value);
			if (!frm->ro) fs->value[0] = 0;
			fs->state = 0;
			break;
		case ACT_EDIT_PASTE_CLIPBOARD: {
			char *clipboard = get_clipboard_text();

			if (!clipboard)
				break;
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
		case ACT_EDIT_ENTER:
			if (frm->type == FC_TEXTAREA) {
				if (textarea_op_enter(fs, frm, rep)) {
					x = 0;
					break;
				}
			} else {
				x = 0;
			}
			break;
		case ACT_EDIT_BACKSPACE:
			if (!frm->ro && fs->state)
				memmove(fs->value + fs->state - 1, fs->value + fs->state,
					strlen(fs->value + fs->state) + 1),
				fs->state--;
			break;
		case ACT_EDIT_DELETE:
			if (!frm->ro && fs->state < strlen(fs->value))
				memmove(fs->value + fs->state, fs->value + fs->state + 1,
					strlen(fs->value + fs->state));
			break;
		case ACT_EDIT_KILL_TO_BOL:
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
		case ACT_EDIT_KILL_TO_EOL:
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

		case ACT_EDIT_REDRAW:
			redraw_terminal_cls(term);
			x = 0;
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
				break;
			}
			x = 0;
			break;
	}

	return x;
}

int
field_op(struct session *ses, struct document_view *doc_view, struct link *l,
	 struct term_event *ev, int rep)
{
	struct form_control *frm;
	struct form_state *fs;
	int x;

	assert(ses && doc_view && l && ev);
	if_assert_failed return 0;
	frm = l->form;
	assertm(frm, "link has no form control");
	if_assert_failed return 0;

	if (l->form->ro == 2) return 0;
	fs = find_form_state(doc_view, frm);
	if (!fs || !fs->value) return 0;

	if (ev->ev == EV_KBD) {
		x = field_op_do(ses->tab->term, doc_view, frm, fs, l, ev, rep);
	} else {
		x = 0;
	}

	if (x) {
		draw_form_entry(ses->tab->term, doc_view, l);
		redraw_from_window(ses->tab);
	}

	return x;
}
