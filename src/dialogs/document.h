/* $Id: document.h,v 1.4 2003/07/05 10:24:40 zas Exp $ */

#ifndef EL__DIALOGS_DOCUMENT_H
#define EL__DIALOGS_DOCUMENT_H

#include "sched/session.h"

/* void loc_msg(struct terminal *, struct location *, struct f_data_c *); */
void nowhere_box(struct terminal *term, unsigned char *title);
void state_msg(struct session *); /* Wrapper for loc_msg(). */
void head_msg(struct session *);

#endif
