/* $Id: dialogs.h,v 1.5 2003/12/01 15:04:15 pasky Exp $ */

#ifndef EL__CACHE_DIALOGS_H
#define EL__CACHE_DIALOGS_H

#include "bfu/hierbox.h"

struct session;
struct terminal;

extern struct hierbox_browser cache_browser;
void menu_cache_manager(struct terminal *, void *, struct session *);

#endif
