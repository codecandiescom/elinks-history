/* Parser of HTTP date */
/* $Id: date.c,v 1.1 2002/03/17 21:53:09 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#ifdef TIME_WITH_SYS_TIME
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#else
#if defined(TM_IN_SYS_TIME) && defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#elif defined(HAVE_TIME_H)
#include <time.h>
#endif
#endif

#include <links.h>


/* TODO: I guess this needs a bit more of rewrite. We can't cope with broken
 * dates (one superfluous/missing character here or there) well enough. */
time_t parse_http_date(const char *date)
{
	/* Mon, 03 Jan 2000 21:29:33 GMT */
	const char *months[12] =
		{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	struct tm tm;
	time_t t = 0;

	if (!date)
		return 0;

	/* Skip day-of-week */

	while (*date && *date != ' ') date++;
	date++;

	if (strlen(date) < 21) {
		/* It's too short! */
		return 0;
	}

	/* Eat day */

	tm.tm_mday = (date[0] - '0') * 10 + date[1] - '0';
	date += 3;

	/* Eat month */

	for (tm.tm_mon = 0; tm.tm_mon < 12; tm.tm_mon++)
		if (!strncmp(date, months[tm.tm_mon], 3))
			break;
	date += 4;

	/* Eat year */

	tm.tm_year = 0;

	if (date[3] == '0' && date[4] == '0') {
		/* Four-digit year */
		tm.tm_year = (date[0] - '0') * 1000 + (date[1] - '0') * 100;
		date += 2;
		/* We take off the 1900 later. */
	}

	tm.tm_year += (date[0] - '0') * 10 + (date[1] - '0');
	date += 3;

	if (tm.tm_year < 60) {
		/* It's already next century. */
		tm.tm_year += 100;
	}

	if (tm.tm_year >= 200) {
		/* Four-digit year, saga continues */
		tm.tm_year -= 1900;
	}

	/* Eat hour */

	tm.tm_hour = (date[0] - '0') * 10 + date[1] - '0';
	date += 3;

	/* Eat minute */

	tm.tm_min = (date[0] - '0') * 10 + date[1] - '0';
	date += 3;

	/* Eat second */

	tm.tm_sec = (date[0] - '0') * 10 + date[1] - '0';

	/* TODO: Maybe we should accept non-GMT times as well? */

#ifdef HAVE_TIMEGM
	t = timegm(&tm);
#else
	/* Since mktime thinks we have localtime, we need a wrapper
	 * to handle GMT. */
	{
		char *tz = getenv("TZ");

		if (tz && *tz) {
			/* Temporary disable timezone in-place. */
			char tmp = *tz;

			*tz = '\0';
			tzset();

			t = mktime(&tm);

			*tz = tmp;
			tzset();

		} else {
			t = mktime(&tm);
		}
	}
#endif

	if (t == (time_t) -1)
		return 0;
	else
		return t;
}
