/* $Id: msgbox.h,v 1.2 2003/05/04 17:25:51 pasky Exp $ */

#ifndef EL__BFU_MSGBOX_H
#define EL__BFU_MSGBOX_H

#include "bfu/align.h"
#include "bfu/button.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

void msg_box(struct terminal *, struct memory_list *, unsigned char *, enum format_align, /*unsigned char *, void *, int,*/ ...);

#endif
