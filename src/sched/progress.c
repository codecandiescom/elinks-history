/* Downloads progression stuff. */
/* $Id: progress.c,v 1.3 2005/04/17 20:18:57 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "sched/progress.h"
#include "util/error.h"
#include "util/memory.h"

struct progress *
init_progress(int start)
{
	struct progress *progress = mem_calloc(1, sizeof(*progress));

	if (progress) {
		progress->start = start;
		progress->timer = TIMER_ID_UNDEF;
	}

	return progress;
}

void
done_progress(struct progress *progress)
{
	mem_free(progress);
}



