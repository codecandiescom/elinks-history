/* $Id: sched.h,v 1.12 2003/07/02 23:40:04 jonas Exp $ */

#ifndef EL__SCHED_SCHED_H
#define EL__SCHED_SCHED_H

#include "document/cache.h"
#include "sched/connection.h"
#include "util/lists.h"

struct status {
	/* XXX: order matters there, there's some hard initialization in
	 * src/sched/session.c and src/viewer/text/view.c */
	LIST_HEAD(struct status);

	struct connection *c;
	struct cache_entry *ce;
	void (*end)(struct status *, void *);
	void *data;
	struct remaining_info *prg;

	int state;
	int prev_error;
	int pri;
};

int load_url(unsigned char *, unsigned char *, struct status *, int, enum cache_mode, int start);

#endif
