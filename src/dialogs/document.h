/* $Id: document.h,v 1.3 2003/01/05 16:48:13 pasky Exp $ */

#ifndef EL__DIALOGS_DOCUMENT_H
#define EL__DIALOGS_DOCUMENT_H

#include "sched/session.h"

/* void loc_msg(struct terminal *, struct location *, struct f_data_c *); */
void state_msg(struct session *); /* Wrapper for loc_msg(). */
void head_msg(struct session *);

#endif
