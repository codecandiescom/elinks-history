/* Implementation of a login manager for HTML forms */
/* $Id: formhist.h,v 1.10 2003/08/03 00:15:49 jonas Exp $ */

#ifndef EL__FORMHIST_FORMHIST_H
#define EL__FORMHIST_FORMHIST_H

#include "util/lists.h"

struct form_history_item {
	LIST_HEAD(struct form_history_item);

	/* List of submitted_values for this form */
	struct list_head submit;

	/* <action> URI for this form. Keep last! */
	unsigned char url[1];
};

/* Queries the form history for the form control value with the given @name on
 * the given site @url. */
/* Returns the saved value if present or NULL */
unsigned char *get_form_history_value(unsigned char *url, unsigned char *name);

/* Queries the user whether to add a new item to the form history containing
 * the submitted values of the form. */
/* Returns the form history item's submit list if the item was successfully
 * added or NULL if not. */
struct list_head *memorize_form(struct session *ses, struct list_head *submit, struct form_control *frm);

/* Closes down the form history system. */
void done_form_history(void);

#endif /* EL__FORMHIST_FORMHIST_H */
