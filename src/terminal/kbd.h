/* $Id: kbd.h,v 1.1 2003/05/04 17:24:00 pasky Exp $ */

#ifndef EL__LOWLEVEL_KBD_H
#define EL__LOWLEVEL_KBD_H

/* The mouse reporting button byte looks like:
 *
 * -ss??bbb
 *  ||  |||
 *  ||  +++---> buttons:
 *  ||          0 left
 *  ||          1 middle
 *  ||          2 right
 *  ||          3 release OR wheel up [rxvt]
 *  ||          4 wheel down [rxvt]
 *  ||
 *  ++--------> style:
 *              1 normalclick (makes sure the whole thing is >= ' ')
 *              2 doubleclick
 *              3 mouse wheel move [xterm]
 *                then in bb is 0 for up and 1 for down
 *
 * What we translate it to:
 * -da??bbb
 *  ||  |||
 *  ||  +++---> buttons:
 *  ||          0 left
 *  ||          1 middle
 *  ||          2 right
 *  ||          3 wheel up
 *  ||          4 wheel down
 *  ||
 *  |+--------> action:
 *  |           0 press (B_DOWN)
 *  |           1 release (B_UP)
 *  |
 *  +---------> drag flag (valid only for B_UP)
 *
 * (TODO: doubleclick facility? --pasky)
 *
 * Let me introduce to the wonderful world of X terminals and their handling of
 * mouse wheel now. First, one sad fact: reasonable mouse wheel reporting
 * support has only xterm itself (from everything I tried) - it is relatively
 * elegant, it shouldn't confuse existing applications too much and it is
 * generally non-conflicting with the old behaviour.
 *
 * All other terminals are doing absolutely terrible things, making it hell on
 * the hearth to support mouse wheels. rxvt will send the buttons as succeeding
 * button numbers (3, 4) in normal sequences, but that has a little problem -
 * button 3 is also alias for button release; welcome to the wonderful world of
 * X11.  But at least, rxvt will send only "press" sequence, but no release
 * sequence (it really doesn't make sense for wheels anyway, does it?). That
 * has the advantage that you can do some heuristic (see below) to at least
 * partially pace with this terrible thing.
 *
 * But now, let's see another nice pair of wonderful terminals: aterm and
 * Eterm. They emit same braindead sequence for wheel up/down as rxvt, but
 * that's not all. Yes, you guessed it - they send even the release sequence
 * (immediatelly after press sequence) for the wheels. So, you will see
 * something like:
 *
 * old glasses - <button1 press> <release> <release> <release>
 * new glasses - <button1 press> <wheelup press> <wheelup press> <wheelup press>
 * smartglasses1-<button1 press> <release> <wheelup press> <wheelup press>
 * smartglasses2-<button1 press> <release> <wheelup press> <release>
 *
 * But with smartglasses2, you will have a problem with rxvt when someone will
 * move the wheel multiple times - only half of the times it will be recorded.
 * Poof. No luck :-(.
 *
 * [smartglasses1]:
 * When user presses some button and then moves the wheel, action for button
 * release will be done and when he will release the button, action for the
 * wheel will be done. That's unfortunately inevitable in order to work under
 * rxvt :-(.
 */

#define BM_BUTT		7
#define B_LEFT		0
#define B_MIDDLE	1
#define B_RIGHT		2
#define B_WHEEL_UP	3
#define B_WHEEL_DOWN	4

#define BM_ACT		32
#define B_DOWN		0
#define B_UP		32

#define BM_DRAG		64
#define B_DRAG		64

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
