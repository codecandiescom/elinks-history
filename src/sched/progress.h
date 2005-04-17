/* $Id: progress.h,v 1.4 2005/04/17 20:18:57 zas Exp $ */

#ifndef EL__SCHED_PROGRESS_H
#define EL__SCHED_PROGRESS_H

#include "lowlevel/timers.h" /* timer_id_T */
#include "util/time.h"

struct progress {
	time_T elapsed;
	timeval_T last_time;
	time_T dis_b;

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
	int data_in_secs[CURRENT_SPD_SEC];
};

struct progress *init_progress(int start);
void done_progress(struct progress *progress);

#endif
