/* $Id: formhist.h,v 1.29 2005/05/22 02:21:03 miciah Exp $ */

#ifndef EL__FORMHIST_FORMHIST_H
#define EL__FORMHIST_FORMHIST_H

#include "document/forms.h"
#include "modules/module.h"
#include "sched/session.h"
#include "util/lists.h"
#include "util/object.h"

struct formhist_data {
	LIST_HEAD(struct formhist_data);

	/* List of submitted_values for this form */
	struct list_head *submit;

	/* This is indeed maintained by formhist.c, not dialogs.c; much easier
	 * and simpler. */
	struct listbox_item *box_item;
	struct object object;

	/* Whether to save this form or not. */
	unsigned int dontsave:1;

	/* <action> URI for this form. Must be at end of struct. */
	unsigned char url[1];
};

/* Look up @name form of @url document in the form history. Returns the saved
 * value if present, NULL upon an error. */
unsigned char *get_form_history_value(unsigned char *url, unsigned char *name);

void memorize_form(struct session *ses, struct list_head *submit, struct form *forminfo);

int save_forms_to_file(void);
void free_form(struct formhist_data *form);
int load_forms_from_file(void);

extern struct module forms_history_module;

#endif /* EL__FORMHIST_FORMHIST_H */
