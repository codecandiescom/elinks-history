/* $Id: formhist.h,v 1.18 2003/10/26 18:41:24 jonas Exp $ */

#ifndef EL__FORMHIST_FORMHIST_H
#define EL__FORMHIST_FORMHIST_H

#include "document/html/parser.h"
#include "modules/module.h"
#include "sched/session.h"
#include "util/lists.h"

struct formhist_data {
	LIST_HEAD(struct formhist_data);

	/* List of submitted_values for this form */
	struct list_head submit;

	/* <action> URI for this form. Must be at end of struct. */
	unsigned char url[1];
};

/* Look up @name form of @url document in the form history. Returns the saved
 * value if present, NULL upon an error. */
unsigned char *get_form_history_value(unsigned char *url, unsigned char *name);

void memorize_form(struct session *ses, struct list_head *submit, struct form_control *frm);

extern struct module forms_history_module;

#endif /* EL__FORMHIST_FORMHIST_H */
