/* Implementation of a login manager for HTML forms */
/* $Id: formhist.h,v 1.7 2003/08/02 17:16:47 jonas Exp $ */

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

/* Queries the form history for the form control value with the given @name on
 * the given site @url. */
/* Returns the saved value if present or NULL */
unsigned char *get_form_history_value(unsigned char *url, unsigned char *name);
struct list_head *memorize_form(struct session *ses, struct list_head *submit, struct form_control *frm);

void done_form_history(void);

#endif /* EL__FORMSMEM_FORMSMEM_H */
