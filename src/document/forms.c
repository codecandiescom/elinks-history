/* The document base functionality */
/* $Id: forms.c,v 1.2 2004/12/18 01:42:18 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listmenu.h"
#include "document/forms.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"


struct form *
init_form(void)
{
	struct form *form = mem_calloc(1, sizeof(struct form));
	
	if (!form) return NULL;
	init_list(form->items);
	return form;
}

void
done_form(struct form *form)
{
	struct form_control *fc;

	mem_free_if(form->action);
	mem_free_if(form->name);
	mem_free_if(form->target);

	foreach (fc, form->items) {
		done_form_control(fc);
	}
}

int
has_form_submit(struct form *form)
{
	struct form_control *fc;
	int found = 0;

	assert(form);
	if_assert_failed return 0;

	foreach (fc, form->items) {
		found = 1;
		if (fc->type == FC_SUBMIT || fc->type == FC_IMAGE)
			break;
	}

	assertm(found, "form is not on list");
	/* Return path :-). */
	return found;
}


void
done_form_control(struct form_control *fc)
{
	int i;

	assert(fc);
	if_assert_failed return;

	mem_free_if(fc->name);
	mem_free_if(fc->alt);
	mem_free_if(fc->default_value);

	for (i = 0; i < fc->nvalues; i++) {
		mem_free_if(fc->values[i]);
		mem_free_if(fc->labels[i]);
	}

	mem_free_if(fc->values);
	mem_free_if(fc->labels);
	if (fc->menu) free_menu(fc->menu);
}
