/* $Id: progress.h,v 1.9 2005/04/18 12:53:50 zas Exp $ */

#ifndef EL__SCHED_PROGRESS_H
#define EL__SCHED_PROGRESS_H

#include "lowlevel/timers.h" /* timer_id_T */
#include "util/time.h"

#define SPD_DISP_TIME			100
#define CURRENT_SPD_SEC			50
#define CURRENT_SPD_AFTER		100

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
	void (*timer_func)(void *);
	void *timer_func_data;

	int data_in_secs[CURRENT_SPD_SEC];
};

struct progress *init_progress(int start);
void done_progress(struct progress *progress);
void update_progress(struct progress *progress, int loaded, int size, int pos);
void start_update_progress(struct progress *progress, void (*timer_func)(void *), void *timer_func_data);

#define significant_progress(progress) ((progress)->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME)

#define average_speed(progress) \
	((longlong) (progress)->loaded * 10 / ((progress)->elapsed / 100))

#define current_speed(progress) \
	((progress)->cur_loaded / (CURRENT_SPD_SEC * SPD_DISP_TIME / 1000))

#define estimated_time(progress) \
	(((progress)->size - (progress)->pos) \
	 / ((longlong) (progress)->loaded * 10 / ((progress)->elapsed / 100)) \
	 * 1000)

#endif
