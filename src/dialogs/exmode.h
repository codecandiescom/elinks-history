/* $Id: exmode.h,v 1.2 2004/01/26 05:50:17 jonas Exp $ */

#ifndef EL__DIALOGS_EXMODE_H
#define EL__DIALOGS_EXMODE_H

struct session;
struct input_history;
 
extern struct input_history exmode_history;

void exmode_start(struct session *ses);

#endif
