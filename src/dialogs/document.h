/* $Id: document.h,v 1.1 2002/04/28 11:17:29 pasky Exp $ */

#ifndef EL__DIALOGS_DOCUMENT_H
#define EL__DIALOGS_DOCUMENT_H

#include <document/session.h>

/* void loc_msg(struct terminal *, struct location *, struct f_data_c *); */
void state_msg(struct session *); /* Wrapper for loc_msg(). */
void head_msg(struct session *);

#endif
