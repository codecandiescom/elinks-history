/* $Id: progress.h,v 1.18 2005/04/19 22:16:33 zas Exp $ */

#ifndef EL__SCHED_PROGRESS_H
#define EL__SCHED_PROGRESS_H

#include "lowlevel/timers.h" /* timer_id_T */
#include "util/time.h"

#define CURRENT_SPD_SEC 	50	/* number of seconds */

struct progress {
	timeval_T elapsed;
	timeval_T last_time;
	timeval_T dis_b;

	unsigned int valid:1;
	int size, loaded, last_loaded, cur_loaded;

	/* This is offset where the download was resumed possibly */
	/* progress->start == -1 means normal session, not download
	 *            ==  0 means download
	 *             >  0 means resume
	 * --witekfl */
	int start;
	/* This is absolute position in the stream
	 * (relative_position = pos - start) (maybe our fictional
	 * relative_position is equiv to loaded, but I'd rather not rely on it
	 * --pasky). */
	int pos;
	/* If this is non-zero, it indicates that we should seek in the
	 * stream to the value inside before the next write (and zero this
	 * counter then, obviously). */
	int seek;

	timer_id_T timer;
	void (*timer_func)(void *);
	void *timer_func_data;

	int data_in_secs[CURRENT_SPD_SEC];
};

struct progress *init_progress(int start);
void done_progress(struct progress *progress);
void update_progress(struct progress *progress, int loaded, int size, int pos);
void start_update_progress(struct progress *progress, void (*timer_func)(void *), void *timer_func_data);

#define progress_elapsed_in_ms(progress) (timeval_to_milliseconds(&(progress)->elapsed))

int has_progress(struct progress *progress);
int progress_average_speed(struct progress *progress);
int progress_current_speed(struct progress *progress);
int progress_estimated_time(struct progress *progress);

#endif
