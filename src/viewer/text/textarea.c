/* Textarea form item handlers */
/* $Id: textarea.c,v 1.133 2004/06/23 14:02:37 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE /* XXX: we want memrchr() ! */

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
	int start;
	int end;
};

/* We add two extra entries to the table so the ending info can be added
 * without reallocating. */
#define realloc_line_info(info, size) \
	mem_align_alloc(info, size, (size) + 3, struct line_info, 0xFF)

/* Allocates a line_info table describing the layout of the textarea buffer.
 *
 * @width	is max width and the offset at which text will be wrapped
 * @wrap	controls how the wrapping of text is performed
 * @format	is non zero the @text will be modified to make it suitable for
 *		encoding it for form posting
 */
static struct line_info *
format_text(unsigned char *text, int width, enum form_wrap wrap, int format)
{
	struct line_info *line = NULL;
	int line_number = 0;
	int begin = 0;
	int pos = 0;
	int skip;

	assert(text);
	if_assert_failed return NULL;

	/* Allocate the ending entries */
	if (!realloc_line_info(&line, 0))
		return NULL;

	while (text[pos]) {
		if (text[pos] == '\n') {
			skip = 1;

		} else if (wrap == FORM_WRAP_NONE || pos - begin < width) {
			pos++;
			continue;

		} else {
			unsigned char *wrappos;

			/* Find a place to wrap the text */
			wrappos = memrchr(&text[begin], ' ', pos - begin);
			if (wrappos) {
				/* When formatting text for form submitting we
				 * have to apply the wrapping mode. */
				if (wrap == FORM_WRAP_HARD && format)
					*wrappos = '\n';
				pos = wrappos - text;
			}
			skip = !!wrappos;
		}

		if (!realloc_line_info(&line, line_number)) {
			mem_free_if(line);
			return NULL;
		}

		line[line_number].start = begin;
		line[line_number++].end = pos;
		begin = pos += skip;
	}

	/* Flush the last text before the loop ended */
	line[line_number].start = begin;
	line[line_number++].end = pos;

	/* Add end marker */
	line[line_number].start = line[line_number].end = -1;

	return line;
}

/* Searches for @cursor_position (aka. position in the fs->value string) for
 * the corresponding entry in the @line info. Returns the index or -1 if
 * position is not found. */
static int
get_textarea_line_number(struct line_info *line, int cursor_position)
{
	int idx;

	for (idx = 0; line[idx].start != -1; idx++) {
		int wrap;

		if (cursor_position < line[idx].start) continue;

		wrap = (line[idx + 1].start == line[idx].end);
		if (cursor_position >= line[idx].end + !wrap) continue;

		return idx;
	}

	return -1;
}

/* Fixes up the vpos and vypos members of the form_state. Returns the
 * logical position in the textarea view. */
int
area_cursor(struct form_control *fc, struct form_state *fs)
{
	struct line_info *line;
	int x, y;

	assert(fc && fs);
	if_assert_failed return 0;

	line = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!line) return 0;

	y = get_textarea_line_number(line, fs->state);
	if (y == -1) {
		mem_free(line);
		return 0;
	}

	x = fs->state - line[y].start;

	mem_free(line);

	if (fc->wrap && x == fc->cols) x--;

	int_bounds(&fs->vpos, x - fc->cols + 1, x);
	int_bounds(&fs->vypos, y - fc->rows + 1, y);

	x -= fs->vpos;
	y -= fs->vypos;

	return y * fc->cols + x;
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
	linex = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!linex) return;
	line = linex;
	sl = fs->vypos;
	while (line->start != -1 && sl) sl--, line++;

	x = link->points[0].x + box->x - vx;
	y = link->points[0].y + box->y - vy;
	ye = y + fc->rows;

	for (; line->start != -1 && y < ye; line++, y++) {
		register int i;

		if (!row_is_in_box(box, y)) continue;

		for (i = 0; i < fc->cols; i++) {
			unsigned char data;
			int xi = x + i;

			if (!col_is_in_box(box, xi))
				continue;

			if (i >= -fs->vpos
			    && i + fs->vpos < line->end - line->start)
				data = fs->value[line->start + i + fs->vpos];
			else
				data = '_';

			draw_char_data(term, xi, y, data);
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
	struct form_control *fc;
	struct string newtext;
	void *blabla;
	register int i;

	assert(sv && sv->value);
	if_assert_failed return NULL;

	fc = sv->form_control;

	/* We need to reformat text now if it has to be wrapped hard, just
	 * before encoding it. */
	blabla = format_text(sv->value, fc->cols, fc->wrap, 1);
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


/* We use some evil hacking in order to make external textarea editor working.
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
 * *NEEDS* rewrite, as it looks just like quick messy hack now. --pasky */

int textarea_editor = 0;

static unsigned char *
save_textarea_file(unsigned char *value)
{
	unsigned char *filename;
	FILE *file = NULL;
	int h;

	filename = get_tempdir_filename("elinks-area-XXXXXX");
	if (!filename) return NULL;

	h = safe_mkstemp(filename);
	if (h >= 0) file = fdopen(h, "w");

	if (file) {
		fwrite(value, strlen(value), 1, file);
		fclose(file);
	} else {
		mem_free(filename);
	}

	return filename;
}

static unsigned char *
load_textarea_file(unsigned char *filename, int maxlength)
{
	unsigned char *value = NULL;
	FILE *file = fopen(filename, "r+");
	int filelen = -1;

	if (!file) return NULL;

	if (!fseek(file, 0, SEEK_END)) {
		filelen = ftell(file);
		if (filelen != -1 && fseek(file, 0, SEEK_SET))
			filelen = -1;
	}

	if (filelen >= 0 && filelen <= maxlength) {
		int bread;

		value = mem_alloc(filelen + 1);
		if (value) {
			bread = fread(value, 1, filelen, file);
			value[bread] = 0;
		}
	}

	fclose(file);
	unlink(filename);

	return value;
}

void
textarea_edit(int op, struct terminal *term_, struct form_state *fs_,
	      struct document_view *doc_view_, struct link *link_)
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

	if (op == 0 && get_cmd_opt_bool("anonymous")) {
		msg_box(term_, NULL, 0,
			N_("Error"), AL_CENTER,
			N_("You cannot launch an external editor in the anonymous mode."),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		return;
	}

	if (op == 0 && !term_->master) {
		msg_box(term_, NULL, 0,
			N_("Error"), AL_CENTER,
			N_("You can do this only on the master terminal"),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		return;
	}

	if (op == 0 && !textarea_editor) {
		unsigned char *ed = getenv("EDITOR");
		unsigned char *ex;

		fn = save_textarea_file(fs_->value);
		if (!fn) return;

		if (!ed || !*ed) ed = "vi";

		ex = straconcat(ed, " ", fn, NULL);
		if (!ex) {
			unlink(fn);
			goto free_and_return;
		}

		if (fs_) fs = fs_;
		if (doc_view_) doc_view = doc_view_;
		if (link_) {
			link = link_;
			fc_maxlength = link_->form_control->maxlength;
		}
		if (term_) term = term_;

		exec_on_terminal(term, ex, "", 1);
		mem_free(ex);

		textarea_editor = 1;

	} else if (op == 1 && fs) {
		unsigned char *value = load_textarea_file(fn, fc_maxlength);

		if (value) {
			mem_free(fs->value);
			fs->value = value;
			fs->state = strlen(value);

			if (doc_view && link)
				draw_form_entry(term, doc_view, link);
		}

		textarea_editor = 0;
		goto free_and_return;
	}

	return;

free_and_return:
	mem_free_set(&fn, NULL);
	fs = NULL;
}

void
menu_textarea_edit(struct terminal *term, void *xxx, struct session *ses)
{
	struct document_view *doc_view;
	struct link *link;
	struct form_state *fs;

	assert(term && ses);
	if_assert_failed return;

	doc_view = current_frame(ses);

	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	link = get_current_link(doc_view);
	if (!link) return;

	if (form_field_is_readonly(link->form_control))
		return;

	fs = find_form_state(doc_view, link->form_control);
	if (!fs) return;

	textarea_edit(0, term, fs, doc_view, link);
}


/* TODO: Unify the textarea field_op handlers to one trampoline function. */

enum frame_event_status
textarea_op_home(struct form_state *fs, struct form_control *fc)
{
	struct line_info *line;
	int current, state;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	state = fs->state;
	line = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!line) return FRAME_EVENT_OK;

	current = get_textarea_line_number(line, fs->state);
	if (current != -1) fs->state = line[current].start;

	mem_free(line);

	return fs->state == state ? FRAME_EVENT_OK : FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_up(struct form_state *fs, struct form_control *fc)
{
	struct line_info *line;
	int current, state;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!line) return FRAME_EVENT_OK;


	current = get_textarea_line_number(line, fs->state);
	if (current == -1) {
		mem_free(line);
		return FRAME_EVENT_OK;
	}

	if (!current) {
		mem_free(line);
		return FRAME_EVENT_IGNORED;
	}

	state = fs->state;
	fs->state -= line[current].start - line[current-1].start;
	int_upper_bound(&fs->state, line[current-1].end);

	mem_free(line);
	return fs->state == state ? FRAME_EVENT_OK : FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_down(struct form_state *fs, struct form_control *fc)
{
	struct line_info *line;
	int current, state;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!line) return FRAME_EVENT_OK;

	current = get_textarea_line_number(line, fs->state);
	if (current == -1) {
		mem_free(line);
		return FRAME_EVENT_OK;
	}

	if (line[current+1].start == -1) {
		mem_free(line);
		return FRAME_EVENT_IGNORED;
	}

	state = fs->state;
	fs->state += line[current+1].start - line[current].start;
	int_upper_bound(&fs->state, line[current+1].end);

	mem_free(line);
	return fs->state == state ? FRAME_EVENT_OK : FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_end(struct form_state *fs, struct form_control *fc)
{
	struct line_info *line;
	int current, state;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	state = fs->state;
	line = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!line) return FRAME_EVENT_OK;

	current = get_textarea_line_number(line, fs->state);
	if (current == -1) {
		fs->state = strlen(fs->value);

	} else {
		int wrap = line[current + 1].start == line[current].end;

		/* Don't jump to next line when wrapping. */
		fs->state = int_max(0, line[current].end - wrap);
	}

	mem_free(line);
	return fs->state == state ? FRAME_EVENT_OK : FRAME_EVENT_REFRESH;
}

/* Set the form state so the cursor is on the first line of the buffer.
 * Preserve the column if possible. */
enum frame_event_status
textarea_op_bob(struct form_state *fs, struct form_control *fc)
{
	struct line_info *line;
	int current, state;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	state = fs->state;
	line = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!line) return FRAME_EVENT_OK;

	current = get_textarea_line_number(line, fs->state);
	if (current != -1) {
		fs->state -= line[current].start;
		int_upper_bound(&fs->state, line[0].end);
	}

	mem_free(line);
	return fs->state == state ? FRAME_EVENT_OK : FRAME_EVENT_REFRESH;
}

/* Set the form state so the cursor is on the last line of the buffer. Preserve
 * the column if possible. This is done by getting current and last line and
 * then shifting the state by the delta of both lines start position bounding
 * the whole thing to the end of the last line. */
enum frame_event_status
textarea_op_eob(struct form_state *fs, struct form_control *fc)
{
	struct line_info *line;
	int current, state;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	state = fs->state;
	line = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!line) return FRAME_EVENT_OK;

	current = get_textarea_line_number(line, fs->state);
	if (current == -1) {
		fs->state = strlen(fs->value);

	} else {
		int last = get_textarea_line_number(line, strlen(fs->value));

		assertm(last != -1, "line info corrupt");

		fs->state += line[last].start - line[current].start;
		int_upper_bound(&fs->state, line[last].end);
	}

	mem_free(line);
	return fs->state == state ? FRAME_EVENT_OK : FRAME_EVENT_REFRESH;
}

enum frame_event_status
textarea_op_enter(struct form_state *fs, struct form_control *fc)
{
	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	if (form_field_is_readonly(fc)
	    || strlen(fs->value) >= fc->maxlength
	    || !insert_in_string(&fs->value, fs->state, "\n", 1))
		return FRAME_EVENT_OK;

	fs->state++;
	return FRAME_EVENT_REFRESH;
}


void
set_textarea(struct document_view *doc_view, int direction)
{
	struct form_control *fc;
	struct form_state *fs;
	struct link *link;

	assert(doc_view && doc_view->vs && doc_view->document);
	assert(direction == 1 || direction == -1);
	if_assert_failed return;

	link = get_current_link(doc_view);
	if (!link || link->type != LINK_AREA)
		return;

	fc = link->form_control;
	assertm(fc, "link has no form control");
	if_assert_failed return;

	if (fc->mode == FORM_MODE_DISABLED) return;

	fs = find_form_state(doc_view, fc);
	if (!fs || !fs->value) return;

	/* Depending on which way we entered the textarea move cursor so that
	 * it is available at end or start. */
	if (direction == 1)
		textarea_op_eob(fs, fc);
	else
		textarea_op_bob(fs, fc);
}
