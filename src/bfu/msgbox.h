/* $Id: msgbox.h,v 1.1 2002/07/04 01:07:12 pasky Exp $ */

#ifndef EL__BFU_MSGBOX_H
#define EL__BFU_MSGBOX_H

#include "bfu/align.h"
#include "bfu/button.h"
#include "lowlevel/terminal.h"
#include "util/memlist.h"

void msg_box(struct terminal *, struct memory_list *, unsigned char *, enum format_align, /*unsigned char *, void *, int,*/ ...);

#endif
