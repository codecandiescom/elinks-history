/* Implementation of a login manager for HTML forms */
/* $Id: formhist.h,v 1.3 2003/08/02 15:35:17 jonas Exp $ */

#ifndef EL__FORMSMEM_FORMSMEM_H
#define EL__FORMSMEM_FORMSMEM_H

#include "util/lists.h"

struct form_history_item {
	LIST_HEAD(struct form_history_item);

	/* List of submitted_values for this form */
	struct list_head submit;

	/* <action> URI for this form. Keep last! */
	unsigned char *url;
};

unsigned char *get_saved_control_value(unsigned char *url, unsigned char *name);
int form_already_saved(unsigned char *url, struct list_head *submit);
void dont_remember_form(struct form_history_item *item);
int remember_form(struct form_history_item *item);
struct list_head *memorize_form(struct session *ses, struct list_head *submit, struct form_control *frm);

void free_formsmemory(void);
void free_form(struct form_history_item *item);

#endif /* EL__FORMSMEM_FORMSMEM_H */
