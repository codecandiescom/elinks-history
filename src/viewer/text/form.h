/* $Id: form.h,v 1.26 2004/06/16 07:15:50 miciah Exp $ */

#ifndef EL__VIEWER_TEXT_FORM_H
#define EL__VIEWER_TEXT_FORM_H

#include "sched/action.h" /* enum frame_event_status */
#include "util/lists.h" /* LIST_HEAD */

struct document;
struct document_view;
struct link;
struct menu_item;
struct session;
struct term_event;
struct terminal;

enum form_method {
	FM_GET,
	FM_POST,
	FM_POST_MP,
	FM_POST_TEXT_PLAIN,
};

enum form_type {
	FC_TEXT,
	FC_PASSWORD,
	FC_FILE,
	FC_TEXTAREA,
	FC_CHECKBOX,
	FC_RADIO,
	FC_SELECT,
	FC_SUBMIT,
	FC_IMAGE,
	FC_RESET,
	FC_HIDDEN,
};

struct form_control {
	LIST_HEAD(struct form_control);

	int form_num;
	int ctrl_num;
	int g_ctrl_num;
	int position;
	enum form_type type;

	enum form_method method;
	unsigned char *action;
	unsigned char *target;
	unsigned char *name;
	unsigned char *alt;
	unsigned char *default_value;
	int ro;
	int default_state;
	int size;
	int cols, rows, wrap;
	int maxlength;
	int nvalues;
	unsigned char **values;
	unsigned char **labels;
	struct menu_item *menu;
};

struct form_state {
	int form_num;
	int ctrl_num;
	int g_ctrl_num;
	int position;
	enum form_type type;

	unsigned char *value;
	int state;
	int vpos;
	int vypos;
};

struct submitted_value {
	LIST_HEAD(struct submitted_value);

	unsigned char *name;
	unsigned char *value;

	struct form_control *frm;

	int type;
	int position;
};

struct uri *get_form_uri(struct session *ses, struct document_view *doc_view, struct form_control *fc);

unsigned char *get_form_info(struct session *ses, struct document_view *doc_view);

void selected_item(struct terminal *term, void *pitem, struct session *ses);
struct form_state *find_form_state(struct document_view *doc_view, struct form_control *fc);
int get_current_state(struct session *ses);

enum frame_event_status field_op(struct session *ses, struct document_view *doc_view, struct link *link, struct term_event *ev, int rep);

void draw_form_entry(struct terminal *term, struct document_view *doc_view, struct link *link);
void draw_forms(struct terminal *term, struct document_view *doc_view);

int has_form_submit(struct document *document, struct form_control *fc);

void reset_form(struct session *ses, struct document_view *doc_view, int a);
void submit_form(struct session *ses, struct document_view *doc_view, int do_reload);
void auto_submit_form(struct session *ses);

void done_form_control(struct form_control *fc);

#endif
