/* $Id: formhist.h,v 1.17 2003/09/02 13:55:01 zas Exp $ */

#ifndef EL__FORMHIST_FORMHIST_H
#define EL__FORMHIST_FORMHIST_H

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

void done_form_history(void);
void memorize_form(struct session *ses, struct list_head *submit, struct form_control *frm);

#endif /* EL__FORMHIST_FORMHIST_H */
