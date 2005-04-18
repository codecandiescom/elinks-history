/* Downloads progression stuff. */
/* $Id: progress.c,v 1.11 2005/04/18 23:06:44 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "sched/progress.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/types.h"

#define SPD_DISP_TIME			100	/* milliseconds */
#define CURRENT_SPD_AFTER		100	/* milliseconds */


int
has_progress(struct progress *progress)
{
	return (progress_elapsed_in_ms(progress) >= CURRENT_SPD_AFTER);
}

int
progress_average_speed(struct progress *progress) /* -> bytes/second */
{
	return (longlong) progress->loaded * 10 / (progress_elapsed_in_ms(progress) / 100);
}

int
progress_current_speed(struct progress *progress) /* -> bytes/second */
{
	return progress->cur_loaded / (CURRENT_SPD_SEC * SPD_DISP_TIME / 1000);
}

int
progress_estimated_time(struct progress *progress) /* -> milliseconds */
{
	return 	(progress->size - progress->pos) / (progress_average_speed(progress) * 1000);
}

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

void
update_progress(struct progress *progress, int loaded, int size, int pos)
{
	timeval_T now, elapsed;
	long a;	/* FIXME: milliseconds */

	get_timeval(&now);
	timeval_sub(&elapsed, &progress->last_time, &now);
	a = timeval_to_milliseconds(&elapsed);

	progress->loaded = loaded;
	progress->size = size;
	progress->pos = pos;
	if (progress->size != -1 && progress->size < progress->pos)
		progress->size = progress->pos;

	progress->dis_b += a;
	while (progress->dis_b >= SPD_DISP_TIME * CURRENT_SPD_SEC) {
		progress->cur_loaded -= progress->data_in_secs[0];
		memmove(progress->data_in_secs, progress->data_in_secs + 1,
			sizeof(*progress->data_in_secs) * (CURRENT_SPD_SEC - 1));
		progress->data_in_secs[CURRENT_SPD_SEC - 1] = 0;
		progress->dis_b -= SPD_DISP_TIME;
	}

	progress->data_in_secs[CURRENT_SPD_SEC - 1] += progress->loaded - progress->last_loaded;
	progress->cur_loaded += progress->loaded - progress->last_loaded;
	progress->last_loaded = progress->loaded;
	copy_struct(&progress->last_time, &now);
	progress->elapsed += a;
	install_timer(&progress->timer, SPD_DISP_TIME, progress->timer_func, progress->timer_func_data);
}

void
start_update_progress(struct progress *progress, void (*timer_func)(void *),
		      void *timer_func_data)
{
	if (!progress->valid) {
		struct progress tmp;

		/* Just copy useful fields from invalid progress. */
		memset(&tmp, 0, sizeof(tmp));
		tmp.start = progress->start;
		tmp.seek  = progress->seek;
		tmp.valid = 1;

		memcpy(progress, &tmp, sizeof(*progress));
	}
	get_timeval(&progress->last_time);
	progress->last_loaded = progress->loaded;
	progress->timer_func = timer_func;
	progress->timer_func_data = timer_func_data;
}
