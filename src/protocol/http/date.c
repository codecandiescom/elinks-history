/* Parser of HTTP date */
/* $Id: date.c,v 1.15 2003/09/21 12:24:43 zas Exp $ */

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

#include "elinks.h"

#include "protocol/http/date.h"


/*
 * Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
 * Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
 * Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
 */

/* Return year or -1 if failure and move cursor after the year. */
static int
parse_year(const unsigned char **date_p)
{
	const unsigned char *date = *date_p;
	int year;
	unsigned char c;

	/* TODO: Use strtol() ? ;)) --pasky
	 * Well, if faster only... --Zas */

	c = *date++;
	if (c < '0' || c > '9') return -1;
	year = (c - '0') * 10;

	c = *date++;
	if (c < '0' || c > '9') return -1;
	year += c - '0';

	c = *date++;
	if (c >= '0' && c <= '9') {
		/* Four digits date */
		year = year * 10 + c - '0';

		c = *date++;
		if (c < '0' || c > '9') return -1;
		year = year * 10 + c - '0' - 1900;

	} else if (year < 60) {
		/* It's already next century. */
		year += 100;

		date--; /* Take a step back! */
	}

	*date_p = date;
	return year;
}

/* Return 0 for January, 11 for december, -1 for failure. */
static int
parse_month(const unsigned char *date)
{
#if 0
	const unsigned char *months[12] =
		{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	int i;

	for (i = 0; i < 12; i++)
		if (!strncmp(date, months[i], 3))
			return i;
#endif

	switch (date[0]) {
	case 'J': /* Jan, Jun, Jul */
		if (date[1] == 'a' && date[2] == 'n') return 0; /* Jan */
		if (date[1] == 'u') {
			if (date[2] == 'n') return 5; /* Jun */
			if (date[2] == 'l') return 6; /* Jul */
		}
		break;
	case 'M': /* Mar, May */
		if (date[1] != 'a') break;
		if (date[2] == 'r') return 2; /* Mar */
		if (date[2] == 'y') return 4; /* May */
		break;
	case 'A': /* Apr, Aug */
		if (date[1] == 'p' && date[2] == 'r') return 3; /* Apr */
		if (date[1] == 'u' && date[2] == 'g') return 7; /* Aug */
		break;
	case 'S':
		if (date[1] == 'e' && date[2] == 'p') return 8; /* Sep */
		break;
	case 'O':
		if (date[1] == 'c' && date[2] == 't') return 9; /* Oct */
		break;
	case 'N':
		if (date[1] == 'o' && date[2] == 'v') return 10; /* Nov */
		break;
	case 'D':
		if (date[1] == 'e' && date[2] == 'c') return 11; /* Dec */
		break;
	case 'F':
		if (date[1] == 'e' && date[2] == 'b') return 1; /* Feb */
		break;
	default:
		break;
	}

	return -1;
}

/* Return day number. */
static int
parse_day(const unsigned char **date_p)
{
	const unsigned char *date = *date_p;
	int day;
	unsigned char c;

	/* TODO: Use strtol() ? ;)) --pasky */

	c = *date;
	if (c < '0' || c > '9') return 32;
	day = c - '0';

	c = *++date;
	if (c >= '0' && c <= '9') {
		day = day * 10 + c - '0';
		date++;
	}

	*date_p = date;
	return day;
}

/* Expects HH:MM:SS, with HH <= 23, MM <= 59, SS <= 59.
 * Updates tm and returns 0 on failure, otherwise 1. */
static int
parse_time(const unsigned char *date, struct tm *tm)
{
	unsigned char h1, h2, m1, m2, s1, s2;

	h1 = *date++; if (h1 < '0' || h1 > '9') return 0;
	h2 = *date++; if (h2 < '0' || h2 > '9') return 0;
	if (*date++ != ':') return 0;

	m1 = *date++; if (m1 < '0' || m1 > '9') return 0;
	m2 = *date++; if (m2 < '0' || m2 > '9') return 0;
	if (*date++ != ':') return 0;

	s1 = *date++; if (s1 < '0' || s1 > '9') return 0;
	s2 = *date++; if (s2 < '0' || s2 > '9') return 0;

	tm->tm_hour = (h1 - '0') * 10 + h2 - '0';
	tm->tm_min = (m1 - '0') * 10 + m2 - '0';
	tm->tm_sec = (s1 - '0') * 10 + s2 - '0';

	return (tm->tm_hour <= 23 && tm->tm_min <= 59 && tm->tm_sec <= 59);
}


static time_t
my_timegm(struct tm *tm)
{
	time_t t = 0;

	/* Okay, the next part of the code is somehow problematic. Now, we use
	 * own code for calculating the number of seconds from 1.1.1970,
	 * brought here by SC from w3m. I don't like it a lot, but it's 100%
	 * portable, it's faster and it's shorter. */
#if 0
#ifdef HAVE_TIMEGM
	t = timegm(tm);
#else
	/* Since mktime thinks we have localtime, we need a wrapper
	 * to handle GMT. */
	/* FIXME: It was reported that it doesn't work somewhere :/. */
	{
		unsigned char *tz = getenv("TZ");

		if (tz && *tz) {
			/* Temporary disable timezone in-place. */
			unsigned char tmp = *tz;

			*tz = '\0';
			tzset();

			t = mktime(tm);

			*tz = tmp;
			tzset();

		} else {
			/* Already GMT, cool! */
			t = mktime(tm);
		}
	}
#endif
#else
	/* Following code was borrowed from w3m, and its developers probably
	 * borrowed it from somewhere else as well, altough they didn't bother
	 * to mention that. */ /* Actually, same code appears to be in lynx as
	 * well.. oh well. :) */
	/* See README.timegm for explanation about how this works. */
	tm->tm_mon -= 2;
	if (tm->tm_mon < 0) {
		tm->tm_mon += 12;
		tm->tm_year--;
	}
	tm->tm_mon *= 153; tm->tm_mon += 2;
	tm->tm_year -= 68;

	tm->tm_mday += tm->tm_year * 1461 / 4;
	tm->tm_mday += ((tm->tm_mon / 5) - 672);

	t = ((tm->tm_mday * 60 * 60 * 24) +
	     (tm->tm_hour * 60 * 60) +
	     (tm->tm_min * 60) +
	     tm->tm_sec);
#endif

	if (t == (time_t) -1) return 0;

	return t;
}


ttime
parse_http_date(const unsigned char *date)
{
#define skip_time_sep() \
	if (c != ' ' && c != '-') return 0; \
	while ((c = *date) == ' ' || c == '-') date++;

	struct tm tm;
	unsigned char c;

	if (!date) return 0;

	/* Skip day-of-week */

	while ((c = *date++) != ' ') if (!c) return 0;

	while ((c = *date) == ' ') date++;

	if (c >= '0' && c <= '9') {
		/* RFC 1036 / RFC 1123 */

		/* Eat day */

		/* date++; */
		tm.tm_mday = parse_day(&date);
		if (tm.tm_mday > 31) return 0;

		c = *date++;
		skip_time_sep();

		/* Eat month */

		tm.tm_mon = parse_month(date);
		if (tm.tm_mon < 0) return 0;

		date += 3;
		c = *date++;

		skip_time_sep();

		/* Eat year */

		tm.tm_year = parse_year(&date);
		if (tm.tm_year < 0) return 0;

		if (*date++ != ' ') return 0;
		while ((c = *date) == ' ') date++;

		/* Eat time */

		if (!parse_time(date, &tm)) return 0;

	} else {
		/* ANSI C's asctime() format */

		/* Eat month */

		tm.tm_mon = parse_month(date);
		if (tm.tm_mon < 0) return 0;

		date += 3;
		c = *date++;

		/* I know, we shouldn't allow '-', but who cares ;). --pasky */
		skip_time_sep();

		/* Eat day */

		tm.tm_mday = parse_day(&date);
		if (tm.tm_mday > 31) return 0;

		skip_time_sep();

		/* Eat time */

		if (!parse_time(date, &tm)) return 0;
		date += 9;

		skip_time_sep();

		/* Eat year */

		tm.tm_year = parse_year(&date);
		if (tm.tm_year < 0) return 0;
	}
#undef skip_time_sep

	return (ttime) my_timegm(&tm);
}
