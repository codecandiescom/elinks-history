/* $Id: refresh.h,v 1.4 2005/03/04 17:36:29 zas Exp $ */

#ifndef EL__DOCUMENT_REFRESH_H
#define EL__DOCUMENT_REFRESH_H

#include "lowlevel/timers.h" /* timer_id_T */

struct session;
struct uri;

struct document_refresh {
	timer_id_T timer;
	unsigned long seconds;
	struct uri *uri;
};

struct document_refresh *init_document_refresh(unsigned char *url, unsigned long seconds);
void done_document_refresh(struct document_refresh *refresh);
void kill_document_refresh(struct document_refresh *refresh);
void start_document_refresh(struct document_refresh *refresh, struct session *ses);

#endif
