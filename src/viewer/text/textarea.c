/* Textarea form item handlers */
/* $Id: textarea.c,v 1.55 2004/06/02 10:34:58 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bfu/msgbox.h"
#include "document/document.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/textarea.h"
#include "viewer/text/view.h"


/* FIXME: Add comments!! --Zas */

struct line_info {
	unsigned char *st;
	unsigned char *en;
};

#define realloc_line_info(info, size) \
	mem_align_alloc(info, size, (size) + 1, struct line_info, 0xFF)

static struct line_info *
format_text(unsigned char *text, int width, int wrap)
{
	struct line_info *ln = NULL;
	int lnn = 0;
	unsigned char *b = text;
	int sk, ps = 0;

	assert(text);
	if_assert_failed return NULL;

	while (*text) {
		if (*text == '\n') {
			sk = 1;

		} else if (!wrap || text - b < width) {
			text++;
			continue;

		} else {
			unsigned char *s;

			sk = 0;
			for (s = text; s >= b; s--) if (*s == ' ') {
				if (wrap == 2) *s = '\n';
				text = s;
				sk = 1;
				break;
			}
		}
put:
		if (!realloc_line_info(&ln, lnn)) {
			mem_free_if(ln);
			return NULL;
		}

		ln[lnn].st = b;
		ln[lnn++].en = text;
		b = text += sk;
	}

	if (ps < 2) {
		ps++;
		sk = 0;
		goto put;
	}
	ln[lnn - 1].st = ln[lnn - 1].en = NULL;

	return ln;
}

int
area_cursor(struct form_control *frm, struct form_state *fs)
{
	struct line_info *ln;
	int q = 0;
	int y;

	assert(frm && fs);
	if_assert_failed return 0;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

	for (y = 0; ln[y].st; y++) {
		int x = fs->value + fs->state - ln[y].st;

		if (fs->value + fs->state < ln[y].st ||
		    fs->value + fs->state >= ln[y].en + (ln[y + 1].st != ln[y].en))
			continue;

		if (frm->wrap && x == frm->cols) x--;
		int_lower_bound(&fs->vpos, x - frm->cols + 1);
		int_upper_bound(&fs->vpos, x);
		int_lower_bound(&fs->vypos, y - frm->rows + 1);
		int_upper_bound(&fs->vypos, y);
		x -= fs->vpos;
		y -= fs->vypos;
		q = y * frm->cols + x;
		break;
	}
	mem_free(ln);

	return q;
}

void
draw_textarea(struct terminal *t, struct form_state *fs,
	      struct document_view *doc_view, struct link *l)
{
	struct line_info *ln, *lnx;
	struct form_control *frm;
	struct box *box;
	int vx, vy;
	int sl, ye;
	register int x, y;

	assert(t && doc_view && doc_view->document && doc_view->vs && l);
	if_assert_failed return;
	frm = l->form;
	assertm(frm, "link %d has no form", (int)(l - doc_view->document->links));
	if_assert_failed return;

	box = &doc_view->box;
	vx = doc_view->vs->x;
	vy = doc_view->vs->y;

	if (!l->n) return;
	area_cursor(frm, fs);
	lnx = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!lnx) return;
	ln = lnx;
	sl = fs->vypos;
	while (ln->st && sl) sl--, ln++;

	x = l->pos[0].x + box->x - vx;
	y = l->pos[0].y + box->y - vy;
	ye = y + frm->rows;

	for (; ln->st && y < ye; ln++, y++) {
		register int i;

		if (!row_is_in_box(box, y)) continue;

		for (i = 0; i < frm->cols; i++) {
			int xi = x + i;

			if (col_is_in_box(box, xi)) {
				if (fs->value &&
				    i >= -fs->vpos &&
				    i + fs->vpos < ln->en - ln->st)
					draw_char_data(t, xi, y,
						       ln->st[i + fs->vpos]);
				else
					draw_char_data(t, xi, y, '_');
			}
		}
	}

	for (; y < ye; y++) {
		register int i;

		if (!row_is_in_box(box, y)) continue;

		for (i = 0; i < frm->cols; i++) {
			int xi = x + i;

			if (col_is_in_box(box, xi))
				draw_char_data(t, xi, y, '_');
		}
	}

	mem_free(lnx);
}


unsigned char *
encode_textarea(struct submitted_value *sv)
{
	struct string newtext;
	void *blabla;
	register int i;

	assert(sv && sv->value);
	if_assert_failed return NULL;

	/* We need to reformat text now if it has to be wrapped
	 * hard, just before encoding it. */
	blabla = format_text(sv->value, sv->frm->cols, sv->frm->wrap);
	mem_free_if(blabla);

	if (!init_string(&newtext)) return NULL;

	for (i = 0; sv->value[i]; i++) {
		if (sv->value[i] != '\n')
			add_char_to_string(&newtext, sv->value[i]);
		else
			add_to_string(&newtext, "\r\n");
	}

	return newtext.source;
}


/*
 * We use some evil hacking in order to make external textarea editor working.
 * We need to have some way how to be notified that the editor finished and we
 * should reload content of the textarea.  So we use global variable
 * textarea_editor as a flag whether we have one running, and if we have, we
 * just call textarea_edit(1, ...).  Then we recover our state from static
 * variables, reload content of textarea back from file and clean up.
 *
 * Unfortunately, we can't support calling of editor from non-master links
 * session, as it would be extremely ugly to hack (you would have to transfer
 * the content of it back to master somehow, add special flags for not deleting
 * of 'delete' etc) and I'm not going to do that now. Inter-links communication
 * *NEEDS* rewrite, as it looks just like quick messy hack now. --pasky
 */

int textarea_editor = 0;

void
textarea_edit(int op, struct terminal *term_, struct form_control *form_,
	      struct form_state *fs_, struct document_view *doc_view_, struct link *l_)
{
	static int form_maxlength;
	static struct form_state *fs;
	static struct terminal *term;
	static struct document_view *doc_view;
	static struct link *l;
	static unsigned char *fn;

	assert (op == 0 || op == 1);
	if_assert_failed return;
	assert (op == 1 || term_);
	if_assert_failed return;

	if (op == 0 && get_opt_bool_tree(cmdline_options, "anonymous")) {
		msg_box(term_, NULL, 0,
			N_("Error"), AL_CENTER,
			N_("You cannot launch an external editor in the anonymous mode."),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		goto free_and_return;
	}

	if (op == 0 && !term_->master) {
		msg_box(term_, NULL, 0,
			N_("Error"), AL_CENTER,
			N_("You can do this only on the master terminal"),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		goto free_and_return;
	}

	if (form_) form_maxlength = form_->maxlength;
	if (fs_) fs = fs_;
	if (doc_view_) doc_view = doc_view_;
	if (l_) l = l_;
	if (term_) term = term_;

	if (op == 0 && !textarea_editor) {
		FILE *taf;
		unsigned char *ed = getenv("EDITOR");
		unsigned char *ex;
		int h;

		fn = get_tempdir_filename("linksarea-XXXXXX");
		if (!fn) goto free_and_return;

		h = safe_mkstemp(fn);
		if (h < 0) goto free_and_return;

		taf = fdopen(h, "w");
		if (!taf) goto free_and_return;

		fwrite(fs->value, strlen(fs->value), 1, taf);
		fclose(taf);

		if (!ed || !*ed) ed = "vi";

		ex = straconcat(ed, " ", fn, NULL);
		if (!ex) {
			unlink(fn);
			goto free_and_return;
		}

		exec_on_terminal(term, ex, "", 1);
		mem_free(ex);

		textarea_editor = 1;

	} else if (op == 1 && fs) {
		FILE *taf = fopen(fn, "r+");

		if (taf) {
			int flen = -1;

			if (!fseek(taf, 0, SEEK_END)) {
				flen = ftell(taf);
				if (flen != -1
				    && fseek(taf, 0, SEEK_SET))
					flen = -1;
			}

			if (flen >= 0 && flen <= form_maxlength) {
				int bread;

				mem_free(fs->value);
				fs->value = mem_alloc(flen + 1);
				if (!fs->value) goto close;

				bread = fread(fs->value, 1, flen, taf);
				fs->value[bread] = 0;
				fs->state = bread;

				if (doc_view && l)
					draw_form_entry(term, doc_view, l);
			}

close:
			fclose(taf);
			unlink(fn);
		}

		textarea_editor = 0;
		goto free_and_return;
	}

	return;

free_and_return:
	mem_free_set(&fn, NULL);
	fs = NULL;
}

int
menu_textarea_edit(struct terminal *term, void *xxx, struct session *ses)
{
	struct document_view *doc_view;
	struct link *link;
	struct form_control *frm;
	struct form_state *fs;

	assert(term && ses);
	if_assert_failed return 1;

	doc_view = current_frame(ses);

	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return 1;

	link = get_current_link(doc_view);
	if (!link) return 1;

	frm = link->form;
	assert(frm && frm->type == FC_TEXTAREA);
	if (frm->ro) return 1;

	fs = find_form_state(doc_view, frm);
	if (!fs) return 1;

	textarea_edit(0, term, frm, fs, doc_view, link);
	return 2;
}


/* TODO: Unify the textarea field_op handlers to one trampoline function. */

int
textarea_op_home(struct form_state *fs, struct form_control *frm, int rep)
{
	unsigned char *position = fs->value + fs->state;
	unsigned char *prev_end = NULL;
	struct line_info *ln;
	int y;

	assert(fs && fs->value && frm);
	if_assert_failed return 0;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

	for (y = 0; ln[y].st; prev_end = ln[y].en, y++) {
		if (position >= ln[y].st &&
		    position < ln[y].en + (ln[y].st != prev_end)) {
			fs->state = ln[y].st - fs->value;
			goto x;
		}
	}
	fs->state = 0;

x:
	mem_free(ln);
	return 0;
}

int
textarea_op_up(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *ln;
	int y = 0;

	assert(fs && fs->value && frm);
	if_assert_failed return 0;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

	while (ln[y].st) {
		if (fs->value + fs->state >= ln[y].st &&
		    fs->value + fs->state < ln[y].en + (ln[y+1].st != ln[y].en)) {
			if (!y) {
				mem_free(ln);
				return 1;
			}
			fs->state -= ln[y].st - ln[y-1].st;
			int_upper_bound(&fs->state, ln[y-1].en - fs->value);
			if (!rep) goto xx;
			y = 0;
		} else {
			y++;
		}
	}
	mem_free(ln);
	return 1;

xx:
	mem_free(ln);
	return 0;
}

int
textarea_op_down(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *ln;
	int y = 0;

	assert(fs && fs->value && frm);
	if_assert_failed return 0;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

	while (ln[y].st) {
		if (fs->value + fs->state >= ln[y].st &&
		    fs->value + fs->state < ln[y].en + (ln[y+1].st != ln[y].en)) {
			if (!ln[y+1].st) {
				mem_free(ln);
				return 1;
			}
			fs->state += ln[y+1].st - ln[y].st;
			int_upper_bound(&fs->state, ln[y+1].en - fs->value);
			if (!rep) goto yy;
			y = 0;
		} else {
			y++;
		}
	}
	mem_free(ln);
	return 1;
yy:
	mem_free(ln);
	return 0;
}

int
textarea_op_end(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *ln;
	int y;

	assert(fs && fs->value && frm);
	if_assert_failed return 0;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

	for (y = 0; ln[y].st; y++) {
		if (fs->value + fs->state >= ln[y].st &&
		    fs->value + fs->state < ln[y].en + (ln[y+1].st != ln[y].en)) {
			fs->state = ln[y].en - fs->value;

			/* Don't jump to next line when wrapping. */
			if (fs->state && fs->state < strlen(fs->value)
			    && ln[y+1].st == ln[y].en)
				fs->state--;

			goto yyyy;
		}
	}
	fs->state = strlen(fs->value);
yyyy:
	mem_free(ln);
	return 0;
}

int
textarea_op_bob(struct form_state *fs, struct form_control *frm, int rep)
{
	unsigned char *position = fs->value + fs->state;
	struct line_info *ln;
	int y;

	assert(fs && fs->value && frm);
	if_assert_failed return 0;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

	for (y = 0; ln[y].st; y++) {
		if (position <= ln[y].en) {
			fs->state -= ln[y].st - fs->value;

			position = fs->value + fs->state;
			if (position > ln[0].en)
				fs->state = ln[0].en - fs->value;

			goto x;
		}
	}
	fs->state = 0;

x:
	mem_free(ln);
	return 0;
}

int
textarea_op_eob(struct form_state *fs, struct form_control *frm, int rep)
{
	unsigned char *position = fs->value + fs->state;
	struct line_info *ln;
	int y;

	assert(fs && fs->value && frm);
	if_assert_failed return 0;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

	for (y = 0; ln[y].st; y++) {
		if (position <= ln[y].en) {
			for (; ln[y].st && ln[y + 1].st; y++) {
				fs->state += ln[y].en - ln[y].st + 1;
			}

			position = fs->value + fs->state;
			if (position > ln[y].en)
				fs->state = ln[y].en - fs->value;

			goto x;
		}
	}
	fs->state = strlen(fs->value);

x:
	mem_free(ln);
	return 0;
}

int
textarea_op_enter(struct form_state *fs, struct form_control *frm, int rep)
{
	int value_len;

	assert(fs && fs->value && frm);
	if_assert_failed return 0;

	value_len = strlen(fs->value);
	if (!frm->ro && value_len < frm->maxlength) {
		unsigned char *v = mem_realloc(fs->value, value_len + 2);

		if (v) {
			fs->value = v;
			memmove(v + fs->state + 1, v + fs->state, strlen(v + fs->state) + 1);
			v[fs->state++] = '\n';
		}
	}

	return 0;
}


void
set_textarea(struct session *ses, struct document_view *doc_view, int kbd)
{
	struct link *link;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	link = get_current_link(doc_view);
	if (link && link->type == LINK_AREA) {
		struct term_event ev = INIT_TERM_EVENT(EV_KBD, kbd, 0, 0);

		field_op(ses, doc_view, link, &ev, 1);
	}
}
