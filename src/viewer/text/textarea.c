/* Textarea form item handlers */
/* $Id: textarea.c,v 1.8 2003/07/15 12:52:34 jonas Exp $ */

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
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/window.h"
#include "util/error.h"
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
		unsigned char *s;

		if (*text == '\n') {
			sk = 1;

put:
			if (!(lnn & (ALLOC_GR - 1))) {
				struct line_info *_ln = mem_realloc(ln,
						        (lnn + ALLOC_GR)
							* sizeof(struct line_info));

				if (!_ln) {
					if (ln) mem_free(ln);
					return NULL;
				}
				ln = _ln;
			}
			ln[lnn].st = b;
			ln[lnn++].en = text;
			b = text += sk;
			continue;
		}
		if (!wrap || text - b < width) {
			text++;
			continue;
		}
		for (s = text; s >= b; s--) if (*s == ' ') {
			if (wrap == 2) *s = '\n';
			text = s;
			sk = 1;
			goto put;
		}
		sk = 0;
		goto put;
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
		if (x >= frm->cols + fs->vpos) fs->vpos = x - frm->cols + 1;
		if (x < fs->vpos) fs->vpos = x;
		if (y >= frm->rows + fs->vypos) fs->vypos = y - frm->rows + 1;
		if (y < fs->vypos) fs->vypos = y;
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
	      struct f_data_c *f, struct link *l)
{
	struct line_info *ln, *lnx;
	struct form_control *frm;
	int xp, yp;
	int xw, yw;
	int vx, vy;
	int sl, ye;
	register int x, y;

	assert(t && f && f->document && f->vs && l);
	if_assert_failed return;
	frm = l->form;
	assertm(frm, "link %d has no form", (int)(l - f->document->links));
	if_assert_failed return;

	xp = f->xp;
	yp = f->yp;
	xw = f->xw;
	yw = f->yw;
	vx = f->vs->view_posx;
	vy = f->vs->view_pos;

	if (!l->n) return;
	area_cursor(frm, fs);
	lnx = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!lnx) return;
	ln = lnx;
	sl = fs->vypos;
	while (ln->st && sl) sl--, ln++;

	x = l->pos[0].x + xp - vx;
	y = l->pos[0].y + yp - vy;
	ye = y + frm->rows;

	for (; ln->st && y < ye; ln++, y++) {
		register int i;

		if (y < yp || y >= yp + yw) continue;

		for (i = 0; i < frm->cols; i++) {
			int xi = x + i;

			if (xi >= xp && xi < xp + xw) {
				if (fs->value &&
				    i >= -fs->vpos &&
				    i + fs->vpos < ln->en - ln->st)
					set_only_char(t, xi, y,
						      ln->st[i + fs->vpos]);
				else
					set_only_char(t, xi, y, '_');
			}
		}
	}

	for (; y < ye; y++) {
		register int i;

		if (y < yp || y >= yp + yw) continue;

		for (i = 0; i < frm->cols; i++) {
			int xi = x + i;

			if (xi >= xp && xi < xp + xw)
				set_only_char(t, xi, y, '_');
		}
	}

	mem_free(lnx);
}


unsigned char *
encode_textarea(struct submitted_value *sv)
{
	unsigned char *newtext;
	void *blabla;
	int len = 0;
	register int i;

	assert(sv && sv->value);
	if_assert_failed return NULL;

	/* We need to reformat text now if it has to be wrapped
	 * hard, just before encoding it. */
	blabla = format_text(sv->value, sv->frm->cols, sv->frm->wrap);
	if (blabla) mem_free(blabla);

	newtext = init_str();
	if (!newtext) return NULL;

	for (i = 0; sv->value[i]; i++) {
		if (sv->value[i] != '\n')
			add_chr_to_str(&newtext, &len, sv->value[i]);
		else
			add_to_str(&newtext, &len, "\r\n");
	}

	return newtext;
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
	      struct form_state *fs_, struct f_data_c *f_, struct link *l_)
{
	static int form_maxlength;
	static struct form_state *fs;
	static struct terminal *term;
	static struct f_data_c *f;
	static struct link *l;
	static unsigned char *fn = NULL;

	assert (op == 0 || op == 1);
	if_assert_failed return;
	assert (op == 1 || term_);
	if_assert_failed return;

	if (op == 0 && !term_->master) {
		msg_box(term_, NULL, 0,
			N_("Error"), AL_CENTER,
			N_("You can do this only on the master terminal"),
			NULL, 1,
			N_("Cancel"), NULL, B_ENTER | B_ESC);
		goto free_and_return;
	}

	if (form_) form_maxlength = form_->maxlength;
	if (fs_) fs = fs_;
	if (f_) f = f_;
	if (l_) l = l_;
	if (term_) term = term_;

	if (op == 0 && !textarea_editor) {
		FILE *taf;
		unsigned char *ed = getenv("EDITOR");
		unsigned char *ex;
		int h;

		fn = stracpy("linksarea-XXXXXX");
		if (!fn) goto free_and_return;

		h = mkstemp(fn);
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

				if (f && l)
					draw_form_entry(term, f, l);
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
	if (fn) mem_free(fn), fn = NULL;
	fs = NULL;
}


/* TODO: Unify the textarea field_op handlers to one trampoline function. */

int
textarea_op_home(struct form_state *fs, struct form_control *frm, int rep)
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
	int y;

	assert(fs && fs->value && frm);
	if_assert_failed return 0;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

rep1:
	for (y = 0; ln[y].st; y++) {
		if (fs->value + fs->state >= ln[y].st &&
		    fs->value + fs->state < ln[y].en + (ln[y+1].st != ln[y].en)) {
			if (!y) {
				mem_free(ln);
				return 1;
			}
			fs->state -= ln[y].st - ln[y-1].st;
			if (fs->value + fs->state > ln[y-1].en)
				fs->state = ln[y-1].en - fs->value;
			goto xx;
		}
	}
	mem_free(ln);
	return 1;

xx:
	if (rep) goto rep1;
	mem_free(ln);
	return 0;
}

int
textarea_op_down(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *ln;
	int y;

	assert(fs && fs->value && frm);
	if_assert_failed return 0;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

rep2:
	for (y = 0; ln[y].st; y++) {
		if (fs->value + fs->state >= ln[y].st &&
		    fs->value + fs->state < ln[y].en + (ln[y+1].st != ln[y].en)) {
			if (!ln[y+1].st) {
				mem_free(ln);
				return 1;
			}
			fs->state += ln[y+1].st - ln[y].st;
			if (fs->value + fs->state > ln[y+1].en)
				fs->state = ln[y+1].en - fs->value;
			goto yy;
		}
	}
	mem_free(ln);
	return 1;
yy:
	if (rep) goto rep2;
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
set_textarea(struct session *ses, struct f_data_c *f, int kbd)
{
	assert(ses && f && f->vs && f->document);
	if_assert_failed return;

	if (f->vs->current_link != -1
	    && f->document->links[f->vs->current_link].type == L_AREA) {
		struct event ev = { EV_KBD, 0, 0, 0 };

		ev.x = kbd;
		field_op(ses, f, &f->document->links[f->vs->current_link], &ev, 1);
	}
}
