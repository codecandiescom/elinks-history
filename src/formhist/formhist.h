/* $Id: formhist.h,v 1.14 2003/08/29 21:29:36 pasky Exp $ */

#ifndef EL__FORMHIST_FORMHIST_H
#define EL__FORMHIST_FORMHIST_H

#include "util/lists.h"

struct formhist_data {
	LIST_HEAD(struct formhist_data);

	/* List of submitted_values for this form */
	struct list_head *submit;

	/* <action> URI for this form */
	unsigned char *url;
};

/* Look up @name form of @url document in the form history. Returns the saved
 * value if present, NULL upon an error. */
unsigned char *get_form_history_value(unsigned char *url, unsigned char *name);

/* Check whether the form (chain of @submit submitted_values at @url document)
 * is already present in the form history. */
int form_already_saved(unsigned char *url, struct list_head *submit);

void dont_remember_form(struct formhist_data *form);

/* Appends form data @form1 (url and submitted_value(s)) to the password file.
 * Returns 1 on success, 0 otherwise. */
int remember_form(struct formhist_data *form);

void done_form_history(void);
void free_form(struct formhist_data *form);
struct list_head *memorize_form(struct session *ses, struct list_head *submit, struct form_control *frm);

#endif /* EL__FORMHIST_FORMHIST_H */
