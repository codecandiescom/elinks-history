/* Downloads progression stuff. */
/* $Id: progress.c,v 1.17 2005/04/19 23:32:42 zas Exp $ */

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

#define progress_elapsed_in_ms(progress) (timeval_to_milliseconds(&(progress)->elapsed))

int
has_progress(struct progress *progress)
{
	return (progress_elapsed_in_ms(progress) >= CURRENT_SPD_AFTER);
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
	int bytes_delta;
	timeval_T now, elapsed, dis_b_max, dis_b_interval;

	get_timeval(&now);
	timeval_sub(&elapsed, &progress->last_time, &now);
	copy_struct(&progress->last_time, &now);

	progress->loaded = loaded;
	bytes_delta = progress->loaded - progress->last_loaded;
	progress->last_loaded = progress->loaded;

	progress->size = size;
	progress->pos = pos;
	if (progress->size != -1 && progress->size < progress->pos)
		progress->size = progress->pos;

	timeval_add_interval(&progress->elapsed, &elapsed);

	timeval_add_interval(&progress->dis_b, &elapsed);
	milliseconds_to_timeval(&dis_b_max, SPD_DISP_TIME * CURRENT_SPD_SEC);
	milliseconds_to_timeval(&dis_b_interval, SPD_DISP_TIME);

	while (timeval_cmp(&progress->dis_b, &dis_b_max) >= 0) {
		progress->cur_loaded -= progress->data_in_secs[0];
		memmove(progress->data_in_secs, progress->data_in_secs + 1,
			sizeof(*progress->data_in_secs) * (CURRENT_SPD_SEC - 1));
		progress->data_in_secs[CURRENT_SPD_SEC - 1] = 0;
		timeval_sub_interval(&progress->dis_b, &dis_b_interval);
	}
	progress->data_in_secs[CURRENT_SPD_SEC - 1] += bytes_delta;
	progress->cur_loaded += bytes_delta;

	{
		int elapsed_in_ms = progress_elapsed_in_ms(progress);

		if (elapsed_in_ms / 100)	/* Division by zero risk */
			progress->average_speed = (longlong) progress->loaded * 10 / (elapsed_in_ms / 100);

		progress->current_speed = progress->cur_loaded / (CURRENT_SPD_SEC * SPD_DISP_TIME / 1000);

		if (progress->average_speed)	/* Division by zero risk */
			seconds_to_timeval(&progress->estimated_time,
					   (progress->size - progress->pos) / progress->average_speed);
	}

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
