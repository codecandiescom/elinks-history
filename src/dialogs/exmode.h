/* $Id: exmode.h,v 1.3 2004/02/04 12:51:24 pasky Exp $ */

#ifndef EL__DIALOGS_EXMODE_H
#define EL__DIALOGS_EXMODE_H

struct session;
struct input_history;

extern struct input_history exmode_history;

void exmode_start(struct session *ses);

#endif
