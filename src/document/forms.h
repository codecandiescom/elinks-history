/* $Id: forms.h,v 1.3 2004/12/18 15:13:02 pasky Exp $ */

#ifndef EL__DOCUMENT_FORMS_H
#define EL__DOCUMENT_FORMS_H

#include "util/lists.h"

struct document;
struct menu_item;



enum form_method {
	FORM_METHOD_GET,
	FORM_METHOD_POST,
	FORM_METHOD_POST_MP,
	FORM_METHOD_POST_TEXT_PLAIN,
};

struct form {
	LIST_HEAD(struct form);

	unsigned char *action;
	unsigned char *name;
	unsigned char *target;
	enum form_method method;

	struct list_head items; /* -> struct form_control */

#ifdef CONFIG_ECMASCRIPT
	/* This holds the ECMAScript object attached to this structure. It can
	 * be NULL since the object is created on-demand at the first time some
	 * ECMAScript code accesses it. It is freed automatically by the
	 * garbage-collecting code when the ECMAScript context is over (usually
	 * when the document is destroyed). */
	void *ecmascript_obj;
#endif
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
	FC_BUTTON,
	FC_HIDDEN,
};

enum form_mode {
	FORM_MODE_NORMAL,
	FORM_MODE_READONLY,
	FORM_MODE_DISABLED,
};

#define form_field_is_readonly(field) ((field)->mode != FORM_MODE_NORMAL)

enum form_wrap {
	FORM_WRAP_NONE,
	FORM_WRAP_SOFT,
	FORM_WRAP_HARD,
};

struct form_control {
	LIST_HEAD(struct form_control);

	struct form *form;
	int g_ctrl_num;
	int position;
	enum form_type type;
	enum form_mode mode;

	unsigned char *name;
	unsigned char *alt;
	unsigned char *default_value;
	int default_state;
	int size;
	int cols, rows;
	enum form_wrap wrap;
	int maxlength;
	int nvalues;
	unsigned char **values;
	unsigned char **labels;
	struct menu_item *menu;
};

struct form *init_form(void);
void done_form(struct form *form);
int has_form_submit(struct form *form);

void done_form_control(struct form_control *fc);

#endif
