/* Textarea form item handlers */
/* $Id: textarea.c,v 1.66 2004/06/16 19:30:41 zas Exp $ */

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
	unsigned char *start;
	unsigned char *end;
};

#define realloc_line_info(info, size) \
	mem_align_alloc(info, size, (size) + 1, struct line_info, 0xFF)

static struct line_info *
format_text(unsigned char *text, int width, int wrap)
{
	struct line_info *line = NULL;
	int line_number = 0;
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
		if (!realloc_line_info(&line, line_number)) {
			mem_free_if(line);
			return NULL;
		}

		line[line_number].start = b;
		line[line_number++].end = text;
		b = text += sk;
	}

	if (ps < 2) {
		ps++;
		sk = 0;
		goto put;
	}
	line[line_number - 1].start = line[line_number - 1].end = NULL;

	return line;
}

int
area_cursor(struct form_control *frm, struct form_state *fs)
{
	struct line_info *line;
	int q = 0;
	int y;

	assert(frm && fs);
	if_assert_failed return 0;

	line = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!line) return 0;

	for (y = 0; line[y].start; y++) {
		int x = fs->value + fs->state - line[y].start;

		if (fs->value + fs->state < line[y].start ||
		    fs->value + fs->state >= line[y].end + (line[y + 1].start != line[y].end))
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
	mem_free(line);

	return q;
}

void
draw_textarea(struct terminal *term, struct form_state *fs,
	      struct document_view *doc_view, struct link *link)
{
	struct line_info *line, *linex;
	struct form_control *frm;
	struct box *box;
	int vx, vy;
	int sl, ye;
	register int x, y;

	assert(term && doc_view && doc_view->document && doc_view->vs && link);
	if_assert_failed return;
	frm = link->form_control;
	assertm(frm, "link %d has no form", (int)(link - doc_view->document->links));
	if_assert_failed return;

	box = &doc_view->box;
	vx = doc_view->vs->x;
	vy = doc_view->vs->y;

	if (!link->npoints) return;
	area_cursor(frm, fs);
	linex = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!linex) return;
	line = linex;
	sl = fs->vypos;
	while (line->start && sl) sl--, line++;

	x = link->points[0].x + box->x - vx;
	y = link->points[0].y + box->y - vy;
	ye = y + frm->rows;

	for (; line->start && y < ye; line++, y++) {
		register int i;

		if (!row_is_in_box(box, y)) continue;

		for (i = 0; i < frm->cols; i++) {
			int xi = x + i;

			if (col_is_in_box(box, xi)) {
				if (fs->value &&
				    i >= -fs->vpos &&
				    i + fs->vpos < line->end - line->start)
					draw_char_data(term, xi, y,
						       line->start[i + fs->vpos]);
				else
					draw_char_data(term, xi, y, '_');
			}
		}
	}

	for (; y < ye; y++) {
		register int i;

		if (!row_is_in_box(box, y)) continue;

		for (i = 0; i < frm->cols; i++) {
			int xi = x + i;

			if (col_is_in_box(box, xi))
				draw_char_data(term, xi, y, '_');
		}
	}

	mem_free(linex);
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

		fn = get_tempdir_filename("elinks-area-XXXXXX");
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

	frm = link->form_control;
	assert(frm && frm->type == FC_TEXTAREA);
	if (frm->ro) return 1;

	fs = find_form_state(doc_view, frm);
	if (!fs) return 1;

	textarea_edit(0, term, frm, fs, doc_view, link);
	return 2;
}


/* TODO: Unify the textarea field_op handlers to one trampoline function. */

enum frame_event_status
textarea_op_home(struct form_state *fs, struct form_control *frm, int rep)
{
	unsigned char *position = fs->value + fs->state;
	unsigned char *prev_end = NULL;
	struct line_info *line;
	int y;

	assert(fs && fs->value && frm);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!line) return FRAME_EVENT_OK;

	for (y = 0; line[y].start; prev_end = line[y].end, y++) {
		if (position >= line[y].start &&
		    position < line[y].end + (line[y].start != prev_end)) {
			fs->state = line[y].start - fs->value;
			goto x;
		}
	}
	fs->state = 0;

x:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_up(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *line;
	int y = 0;

	assert(fs && fs->value && frm);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!line) return FRAME_EVENT_OK;

	while (line[y].start) {
		if (fs->value + fs->state >= line[y].start &&
		    fs->value + fs->state < line[y].end + (line[y+1].start != line[y].end)) {
			if (!y) {
				mem_free(line);
				return FRAME_EVENT_OK;
			}
			fs->state -= line[y].start - line[y-1].start;
			int_upper_bound(&fs->state, line[y-1].end - fs->value);
			if (!rep) goto xx;
			y = 0;
		} else {
			y++;
		}
	}
	mem_free(line);
	return FRAME_EVENT_OK;

xx:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_down(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *line;
	int y = 0;

	assert(fs && fs->value && frm);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!line) return FRAME_EVENT_OK;

	while (line[y].start) {
		if (fs->value + fs->state >= line[y].start &&
		    fs->value + fs->state < line[y].end + (line[y+1].start != line[y].end)) {
			if (!line[y+1].start) {
				mem_free(line);
				return FRAME_EVENT_OK;
			}
			fs->state += line[y+1].start - line[y].start;
			int_upper_bound(&fs->state, line[y+1].end - fs->value);
			if (!rep) goto yy;
			y = 0;
		} else {
			y++;
		}
	}
	mem_free(line);
	return FRAME_EVENT_OK;
yy:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_end(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *line;
	int y;

	assert(fs && fs->value && frm);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!line) return FRAME_EVENT_OK;

	for (y = 0; line[y].start; y++) {
		if (fs->value + fs->state >= line[y].start &&
		    fs->value + fs->state < line[y].end + (line[y+1].start != line[y].end)) {
			fs->state = line[y].end - fs->value;

			/* Don't jump to next line when wrapping. */
			if (fs->state && fs->state < strlen(fs->value)
			    && line[y+1].start == line[y].end)
				fs->state--;

			goto yyyy;
		}
	}
	fs->state = strlen(fs->value);
yyyy:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_bob(struct form_state *fs, struct form_control *frm, int rep)
{
	unsigned char *position = fs->value + fs->state;
	struct line_info *line;
	int y;

	assert(fs && fs->value && frm);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!line) return FRAME_EVENT_OK;

	for (y = 0; line[y].start; y++) {
		if (position <= line[y].end) {
			fs->state -= line[y].start - fs->value;

			position = fs->value + fs->state;
			if (position > line[0].end)
				fs->state = line[0].end - fs->value;

			goto x;
		}
	}
	fs->state = 0;

x:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_eob(struct form_state *fs, struct form_control *frm, int rep)
{
	unsigned char *position = fs->value + fs->state;
	struct line_info *line;
	int y;

	assert(fs && fs->value && frm);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!line) return FRAME_EVENT_OK;

	for (y = 0; line[y].start; y++) {
		if (position <= line[y].end) {
			for (; line[y].start && line[y + 1].start; y++) {
				fs->state += line[y].end - line[y].start + 1;
			}

			position = fs->value + fs->state;
			if (position > line[y].end)
				fs->state = line[y].end - fs->value;

			goto x;
		}
	}
	fs->state = strlen(fs->value);

x:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_enter(struct form_state *fs, struct form_control *frm, int rep)
{
	int value_len;

	assert(fs && fs->value && frm);
	if_assert_failed return FRAME_EVENT_OK;

	value_len = strlen(fs->value);
	if (!frm->ro && value_len < frm->maxlength) {
		unsigned char *v = mem_realloc(fs->value, value_len + 2);

		if (v) {
			fs->value = v;
			memmove(v + fs->state + 1, v + fs->state, strlen(v + fs->state) + 1);
			v[fs->state++] = '\n';
		}
	}

	return FRAME_EVENT_REFRESH;
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

		if (field_op(ses, doc_view, link, &ev, 1)
		    == FRAME_EVENT_REFRESH) {
			struct terminal *term = ses->tab->term;

			/* FIXME: I am unsure if this is needed. We get here
			 * called from up() and down() which will both call
			 * refresh_view() which will redraw all form entries
			 * and also call redraw_from_window(). --jonas */
			draw_form_entry(term, doc_view, link);
			redraw_from_window(ses->tab);
		}
	}
}
