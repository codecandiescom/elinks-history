/* Textarea form item handlers */
/* $Id: textarea.c,v 1.81 2004/06/16 21:30:07 zas Exp $ */

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
	unsigned char *begin = text;
	int skip, ps = 0;

	assert(text);
	if_assert_failed return NULL;

	while (*text) {
		if (*text == '\n') {
			skip = 1;

		} else if (!wrap || text - begin < width) {
			text++;
			continue;

		} else {
			unsigned char *s;

			skip = 0;
			for (s = text; s >= begin; s--) if (*s == ' ') {
				if (wrap == 2) *s = '\n';
				text = s;
				skip = 1;
				break;
			}
		}
put:
		if (!realloc_line_info(&line, line_number)) {
			mem_free_if(line);
			return NULL;
		}

		line[line_number].start = begin;
		line[line_number++].end = text;
		begin = text += skip;
	}

	if (ps < 2) {
		ps++;
		skip = 0;
		goto put;
	}
	line[line_number - 1].start = line[line_number - 1].end = NULL;

	return line;
}

int
area_cursor(struct form_control *fc, struct form_state *fs)
{
	unsigned char *position;
	struct line_info *line;
	int q = 0;
	int y;

	assert(fc && fs);
	if_assert_failed return 0;

	line = format_text(fs->value, fc->cols, !!fc->wrap);
	if (!line) return 0;

	position = fs->value + fs->state;

	for (y = 0; line[y].start; y++) {
		int wrap;
		int x = position - line[y].start;

		if (x < 0) continue;

		wrap = (line[y+1].start == line[y].end);
		if (position >= line[y].end + !wrap) continue;

		if (fc->wrap && x == fc->cols) x--;
		int_bounds(&fs->vpos, x - fc->cols + 1, x);
		int_bounds(&fs->vypos, y - fc->rows + 1, y);
		x -= fs->vpos;
		y -= fs->vypos;
		q = y * fc->cols + x;
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
	struct form_control *fc;
	struct box *box;
	int vx, vy;
	int sl, ye;
	register int x, y;

	assert(term && doc_view && doc_view->document && doc_view->vs && link);
	if_assert_failed return;
	fc = link->form_control;
	assertm(fc, "link %d has no form control", (int)(link - doc_view->document->links));
	if_assert_failed return;

	box = &doc_view->box;
	vx = doc_view->vs->x;
	vy = doc_view->vs->y;

	if (!link->npoints) return;
	area_cursor(fc, fs);
	linex = format_text(fs->value, fc->cols, !!fc->wrap);
	if (!linex) return;
	line = linex;
	sl = fs->vypos;
	while (line->start && sl) sl--, line++;

	x = link->points[0].x + box->x - vx;
	y = link->points[0].y + box->y - vy;
	ye = y + fc->rows;

	for (; line->start && y < ye; line++, y++) {
		register int i;

		if (!row_is_in_box(box, y)) continue;

		for (i = 0; i < fc->cols; i++) {
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

		for (i = 0; i < fc->cols; i++) {
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
textarea_edit(int op, struct terminal *term_, struct form_control *fc_,
	      struct form_state *fs_, struct document_view *doc_view_, struct link *link_)
{
	static int fc_maxlength;
	static struct form_state *fs;
	static struct terminal *term;
	static struct document_view *doc_view;
	static struct link *link;
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

	if (fc_) fc_maxlength = fc_->maxlength;
	if (fs_) fs = fs_;
	if (doc_view_) doc_view = doc_view_;
	if (link_) link = link_;
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

			if (flen >= 0 && flen <= fc_maxlength) {
				int bread;

				mem_free(fs->value);
				fs->value = mem_alloc(flen + 1);
				if (!fs->value) goto close;

				bread = fread(fs->value, 1, flen, taf);
				fs->value[bread] = 0;
				fs->state = bread;

				if (doc_view && link)
					draw_form_entry(term, doc_view, link);
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
	struct form_control *fc;
	struct form_state *fs;

	assert(term && ses);
	if_assert_failed return 1;

	doc_view = current_frame(ses);

	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return 1;

	link = get_current_link(doc_view);
	if (!link) return 1;

	fc = link->form_control;
	assert(fc && fc->type == FC_TEXTAREA);
	if (fc->ro) return 1;

	fs = find_form_state(doc_view, fc);
	if (!fs) return 1;

	textarea_edit(0, term, fc, fs, doc_view, link);
	return 2;
}


/* TODO: Unify the textarea field_op handlers to one trampoline function. */

enum frame_event_status
textarea_op_home(struct form_state *fs, struct form_control *fc, int rep)
{
	unsigned char *position;
	unsigned char *prev_end = NULL;
	struct line_info *line;
	int y;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, fc->cols, !!fc->wrap);
	if (!line) return FRAME_EVENT_OK;

	position = fs->value + fs->state;

	for (y = 0; line[y].start; prev_end = line[y].end, y++) {
		int wrap;

		if (position < line[y].start) continue;

		wrap = (line[y].start == prev_end);
		if (position >= line[y].end + !wrap) continue;

		fs->state = line[y].start - fs->value;
		goto free_and_return;
	}
	fs->state = 0;

free_and_return:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_up(struct form_state *fs, struct form_control *fc, int rep)
{
	unsigned char *position;
	struct line_info *line;
	int y;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, fc->cols, !!fc->wrap);
	if (!line) return FRAME_EVENT_OK;

	position = fs->value + fs->state;

	for (y = 0; line[y].start; y++) {
		int wrap;

		if (position < line[y].start) continue;

		wrap = (line[y+1].start == line[y].end);
		if (position >= line[y].end + !wrap) continue;

		if (!y) {
			mem_free(line);
			return FRAME_EVENT_IGNORED;
		}

		fs->state -= line[y].start - line[y-1].start;
		int_upper_bound(&fs->state, line[y-1].end - fs->value);
		if (!rep) goto free_and_return;
		y = -1; /* repeat */
	}
	mem_free(line);
	return FRAME_EVENT_OK;

free_and_return:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_down(struct form_state *fs, struct form_control *fc, int rep)
{
	unsigned char *position;
	struct line_info *line;
	int y;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, fc->cols, !!fc->wrap);
	if (!line) return FRAME_EVENT_OK;

	position = fs->value + fs->state;

	for (y = 0; line[y].start; y++) {
		int wrap;

		if (position < line[y].start) continue;

		wrap = (line[y+1].start == line[y].end);
		if (position >= line[y].end + !wrap) continue;

		if (!line[y+1].start) {
			mem_free(line);
			return FRAME_EVENT_IGNORED;
		}

		fs->state += line[y+1].start - line[y].start;
		int_upper_bound(&fs->state, line[y+1].end - fs->value);
		if (!rep) goto free_and_return;
		y = -1; /* repeat */
	}

	mem_free(line);
	return FRAME_EVENT_OK;

free_and_return:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_end(struct form_state *fs, struct form_control *fc, int rep)
{
	unsigned char *position;
	struct line_info *line;
	int y;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, fc->cols, !!fc->wrap);
	if (!line) return FRAME_EVENT_OK;

	position = fs->value + fs->state;

	for (y = 0; line[y].start; y++) {
		int wrap;

		if (position < line[y].start) continue;

		wrap = (line[y+1].start == line[y].end);
		if (position >= line[y].end + !wrap) continue;

		fs->state = line[y].end - fs->value;

		/* Don't jump to next line when wrapping. */
		if (wrap && fs->state && fs->state < strlen(fs->value))
			fs->state--;

		goto free_and_return;
	}
	fs->state = strlen(fs->value);

free_and_return:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

/* BEGINNING_OF_BUFFER */
enum frame_event_status
textarea_op_bob(struct form_state *fs, struct form_control *fc, int rep)
{
	unsigned char *position;
	struct line_info *line;
	int y;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, fc->cols, !!fc->wrap);
	if (!line) return FRAME_EVENT_OK;

	position = fs->value + fs->state;

	for (y = 0; line[y].start; y++) {
		if (position <= line[y].end) {
			fs->state -= line[y].start - fs->value;

			position = fs->value + fs->state;
			if (position > line[0].end)
				fs->state = line[0].end - fs->value;

			goto free_and_return;
		}
	}
	fs->state = 0;

free_and_return:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

/* END_OF_BUFFER */
enum frame_event_status
textarea_op_eob(struct form_state *fs, struct form_control *fc, int rep)
{
	unsigned char *position;
	struct line_info *line;
	int y;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, fc->cols, !!fc->wrap);
	if (!line) return FRAME_EVENT_OK;

	position = fs->value + fs->state;

	for (y = 0; line[y].start; y++) {
		if (position <= line[y].end) {
			for (; line[y].start && line[y + 1].start; y++) {
				fs->state += line[y].end - line[y].start + 1;
			}

			position = fs->value + fs->state;
			if (position > line[y].end)
				fs->state = line[y].end - fs->value;

			goto free_and_return;
		}
	}
	fs->state = strlen(fs->value);

free_and_return:
	mem_free(line);
	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_enter(struct form_state *fs, struct form_control *fc, int rep)
{
	unsigned char *value;
	int value_len;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	value = fs->value;
	value_len = strlen(value);
	if (!fc->ro && value_len < fc->maxlength) {
		value = mem_realloc(value, value_len + 2);

		if (value) {
			unsigned char *insertpos = &value[fs->state++];

			memmove(insertpos + 1, insertpos, strlen(insertpos) + 1);
			*insertpos = '\n';
			fs->value = value;
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
