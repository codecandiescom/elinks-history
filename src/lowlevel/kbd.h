/* $Id: kbd.h,v 1.2 2002/09/23 10:23:27 pasky Exp $ */

#ifndef EL__KBD_H
#define EL__KBD_H

#define BM_BUTT		3
#define B_LEFT		0
#define B_MIDDLE	1
#define B_RIGHT		2
#define B_WHEEL_UP	3
#define B_WHEEL_DOWN	4
#define BM_ACT		24
#define B_DOWN		0
#define B_UP		8
#define B_DRAG		16

#define KBD_ENTER	0x100
#define KBD_BS		0x101
#define KBD_TAB		0x102
#define KBD_ESC		0x103
#define KBD_LEFT	0x104
#define KBD_RIGHT	0x105
#define KBD_UP		0x106
#define KBD_DOWN	0x107
#define KBD_INS		0x108
#define KBD_DEL		0x109
#define KBD_HOME	0x10a
#define KBD_END		0x10b
#define KBD_PAGE_UP	0x10c
#define KBD_PAGE_DOWN	0x10d

#define KBD_F1		0x120
#define KBD_F2		0x121
#define KBD_F3		0x122
#define KBD_F4		0x123
#define KBD_F5		0x124
#define KBD_F6		0x125
#define KBD_F7		0x126
#define KBD_F8		0x127
#define KBD_F9		0x128
#define KBD_F10		0x129
#define KBD_F11		0x12a
#define KBD_F12		0x12b

#define KBD_CTRL_C	0x200

#define KBD_SHIFT	1
#define KBD_CTRL	2
#define KBD_ALT		4

void handle_trm(int, int, int, int, int, void *, int);
void free_all_itrms();
void resize_terminal();
void dispatch_special(unsigned char *);
void kbd_ctrl_c();
int is_blocked();

#endif
