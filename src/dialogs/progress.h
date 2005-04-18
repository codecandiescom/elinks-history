/* $Id: progress.h,v 1.3 2005/04/18 17:19:37 zas Exp $ */

#ifndef EL__DIALOGS_PROGRESS_H
#define EL__DIALOGS_PROGRESS_H

struct progress;
struct terminal;

unsigned char *get_progress_msg(struct progress *progress, struct terminal *term, int wide, int full, unsigned char *separator);

/* Draws a progress bar meter or progress coloured text depending on whether
 * @text is NULL. If @meter_color is NULL dialog.meter color is used. */
void
draw_progress_bar(struct terminal *term, int x, int y, int width,
		  unsigned char *text, struct color_pair *meter_color,
		  longlong current, longlong total);


#endif
