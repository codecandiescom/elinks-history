/* $Id: marks.h,v 1.1 2003/11/25 22:55:03 pasky Exp $ */

#ifndef EL__VIEWER_TEXT_MARKS_H
#define EL__VIEWER_TEXT_MARKS_H

struct view_state;

struct view_state *get_mark(unsigned char mark);
void set_mark(unsigned char mark, struct view_state *vs);

void free_marks(void);

#endif
