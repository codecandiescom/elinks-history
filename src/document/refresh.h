/* $Id: refresh.h,v 1.1 2003/10/31 02:08:37 jonas Exp $ */

#ifndef EL__DOCUMENT_REFRESH_H
#define EL__DOCUMENT_REFRESH_H

#include "sched/session.h"

struct document_refresh {
	int timer;
	unsigned long seconds;
	unsigned char url[1]; /* XXX: Keep last! */
};

struct document_refresh *init_document_refresh(unsigned char *url, unsigned long seconds);
void done_document_refresh(struct document_refresh *refresh);
void kill_document_refresh(struct document_refresh *refresh);
void start_document_refresh(struct document_refresh *refresh, struct session *ses);

#endif
