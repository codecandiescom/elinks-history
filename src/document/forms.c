/* The document base functionality */
/* $Id: forms.c,v 1.1 2004/12/18 00:27:53 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listmenu.h"
#include "document/document.h"
#include "document/forms.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"


int
has_form_submit(struct document *document, struct form_control *fc)
{
	struct form_control *fc2;
	int found = 0;

	assert(document && fc);
	if_assert_failed return 0;

	foreach (fc2, document->forms) {
		if (fc2->form_num != fc->form_num) continue;
		found = 1;
		if (fc2->type == FC_SUBMIT || fc2->type == FC_IMAGE)
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

	mem_free_if(fc->action);
	mem_free_if(fc->target);
	mem_free_if(fc->name);
	mem_free_if(fc->alt);
	mem_free_if(fc->default_value);
	mem_free_if(fc->formname);

	for (i = 0; i < fc->nvalues; i++) {
		mem_free_if(fc->values[i]);
		mem_free_if(fc->labels[i]);
	}

	mem_free_if(fc->values);
	mem_free_if(fc->labels);
	if (fc->menu) free_menu(fc->menu);
}
