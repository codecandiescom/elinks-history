/* $Id: document.h,v 1.5 2004/06/12 17:29:15 jonas Exp $ */

#ifndef EL__DIALOGS_DOCUMENT_H
#define EL__DIALOGS_DOCUMENT_H

#include "sched/session.h"

void nowhere_box(struct terminal *term, unsigned char *title);
void document_info_dialog(struct session *);
void protocol_header_dialog(struct session *);

#endif
