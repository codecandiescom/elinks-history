/* $Id: refresh.h,v 1.3 2004/04/02 15:03:33 jonas Exp $ */

#ifndef EL__DOCUMENT_REFRESH_H
#define EL__DOCUMENT_REFRESH_H

struct session;
struct uri;

struct document_refresh {
	int timer;
	unsigned long seconds;
	struct uri *uri;
};

struct document_refresh *init_document_refresh(unsigned char *url, unsigned long seconds);
void done_document_refresh(struct document_refresh *refresh);
void kill_document_refresh(struct document_refresh *refresh);
void start_document_refresh(struct document_refresh *refresh, struct session *ses);

#endif
