/* $Id: align.h,v 1.1 2002/03/19 20:40:03 pasky Exp $ */

#ifndef EL__BFU_ALIGN_H
#define EL__BFU_ALIGN_H

/* This enum is pretty ugly, yes ;). */
enum format_align {
	AL_LEFT,
	AL_CENTER,
	AL_RIGHT,
	AL_BLOCK,
	AL_NO,

	AL_MASK = 0x7f,

	/* XXX: DIRTY! For backward compatibility with old menu code: */
	AL_EXTD_TEXT = 0x80,
};

#endif
