/* Implementation of a login manager for HTML forms */
/* $Id: formsmem.h,v 1.2 2003/08/01 18:14:49 jonas Exp $ */

#ifndef EL__FORMSMEM_FORMSMEM_H
#define EL__FORMSMEM_FORMSMEM_H

#include "util/lists.h"

struct formsmem_data {
	LIST_HEAD(struct formsmem_data);

	/* List of submitted_values for this form */
	struct list_head *submit;

	/* <action> URI for this form */
	unsigned char *url;
};

unsigned char *get_saved_control_value(unsigned char *url, unsigned char *name);
int form_already_saved(unsigned char *url, struct list_head *submit);
void dont_remember_form(struct formsmem_data *fmem_data);
int remember_form(struct formsmem_data *fmem_data);

void free_formsmemory(void);
void free_form(struct formsmem_data *fmem_data);

#endif /* EL__FORMSMEM_FORMSMEM_H */
