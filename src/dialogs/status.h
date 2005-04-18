/* $Id: status.h,v 1.12 2005/04/18 17:13:03 zas Exp $ */

#ifndef EL__DIALOGS_STATUS_H
#define EL__DIALOGS_STATUS_H

struct download;
struct session;
struct terminal;

void print_screen_status(struct session *);

void update_status(void);

unsigned char *
get_download_msg(struct download *download, struct terminal *term,
	         int wide, int full, unsigned char *separator);

/* Draws a progress bar meter or progress coloured text depending on whether
 * @text is NULL. If @meter_color is NULL dialog.meter color is used. */
void
draw_progress_bar(struct terminal *term, int x, int y, int width,
		  unsigned char *text, struct color_pair *meter_color,
		  longlong current, longlong total);

#endif
