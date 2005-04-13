/* $Id: status.h,v 1.9 2005/04/13 15:34:54 jonas Exp $ */

#ifndef EL__DIALOGS_STATUS_H
#define EL__DIALOGS_STATUS_H

struct download;
struct session;
struct terminal;

void print_screen_status(struct session *);

void update_status(void);

int download_is_progressing(struct download *download);

unsigned char *
get_download_msg(struct download *download, struct terminal *term,
	         int wide, int full, unsigned char *separator);


#endif
