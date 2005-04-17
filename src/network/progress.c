/* Downloads progression stuff. */
/* $Id: progress.c,v 1.2 2005/04/17 18:46:04 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "sched/progress.h"
#include "util/error.h"
#include "util/memory.h"

struct progress *
init_progress(struct progress **progress_ref, int start)
{
	struct progress *progress = mem_calloc(1, sizeof(*progress));

	if (progress) {
		progress->start = start;
		progress->timer = TIMER_ID_UNDEF;
	}

	if (progress_ref)
		*progress_ref = progress;

	return progress;
}

void
done_progress(struct progress **progress_ref)
{
	assert(progress_ref);
	
	mem_free_set(progress_ref, NULL);
}



