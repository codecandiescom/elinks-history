/* Implementation of a login manager for HTML forms */
/* $Id: formhist.h,v 1.12 2003/08/22 15:07:47 zas Exp $ */

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

unsigned char *get_form_history_value(unsigned char *url, unsigned char *name);
int form_already_saved(unsigned char *url, struct list_head *submit);
void dont_remember_form(struct formhist_data *form);
int remember_form(struct formhist_data *form);

void done_form_history(void);
void free_form(struct formhist_data *form);
struct list_head *memorize_form(struct session *ses, struct list_head *submit, struct form_control *frm);

#endif
