/* FTP directory parsing */
/* $Id: ftpparse.c,v 1.21 2005/02/28 14:17:51 zas Exp $ */

/* These sources aren't the officially distributed version, they are modified
 * by us (ELinks coders) and some other third-party hackers. See ELinks
 * ChangeLog for details about changes we made here, comments below may give
 * you some indices as well. --pasky */

/* NOTE: the following source file has been modified to compile cleanly
 * under gcc-2.95. The functionality should remain unchanged from the public
 * distribution.
 *
 * Sources modified by akishan@cs.stanford.edu on January 3, 2002.
 */

/* ftpparse.c, ftpparse.h: library for parsing FTP LIST responses
 * 20001223
 * D. J. Bernstein, djb@cr.yp.to
 * http://cr.yp.to/ftpparse.html
 *
 * Commercial use is fine, if you let me know what programs you're using this in.
 *
 * Currently covered formats:
 * EPLF.
 * UNIX ls, with or without gid.
 * Microsoft FTP Service.
 * Windows NT FTP Server.
 * VMS.
 * WFTPD.
 * NetPresenz (Mac).
 * NetWare.
 * MSDOS.
 *
 * Definitely not covered:
 * Long VMS filenames, with information split across two lines.
 * NCSA Telnet FTP server. Has LIST = NLST (and bad NLST for directories).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <string.h>

#include "elinks.h"

#include "osdep/ascii.h"
#include "protocol/ftp/ftpparse.h"
#include "util/conv.h"


static long
totai(long year, long month, long mday)
{
	long result;

	if (month >= 2)
		month -= 2;
	else {
		month += 10;
		--year;
	}
	result = (mday - 1) * 10 + 5 + 306 * month;
	result /= 10;
	if (result == 365) {
		year -= 3;
		result = 1460;
	} else
		result += 365 * (year % 4);
	year /= 4;
	result += 1461 * (year % 25);
	year /= 25;
	if (result == 36524) {
		year -= 3;
		result = 146096;
	} else {
		result += 36524 * (year % 4);
	}
	year /= 4;
	result += 146097 * (year - 5);
	result += 11017;

	return result * 86400;
}

static int flagneedbase = 1;
static time_t base;		/* time() value on this OS at the beginning of 1970 TAI */
static long now;		/* current time */
static int flagneedcurrentyear = 1;
static long currentyear;	/* approximation to current year */

static void
initbase(void)
{
	struct tm *t;

	if (!flagneedbase)
		return;

	base = 0;
	t = gmtime(&base);
	base = -(totai(t->tm_year + 1900, t->tm_mon, t->tm_mday) +
		 t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec);
	/* assumes the right time_t, counting seconds. */
	/* base may be slightly off if time_t counts non-leap seconds. */
	flagneedbase = 0;
}

static void
initnow(void)
{
	long day;
	long year;

	initbase();
	now = time((time_t *) 0) - base;

	if (!flagneedcurrentyear)
		return;

	day = now / 86400;
	if ((now % 86400) < 0)
		--day;
	day -= 11017;
	year = 5 + day / 146097;
	day = day % 146097;
	if (day < 0) {
		day += 146097;
		--year;
	}
	year *= 4;
	if (day == 146096) {
		year += 3;
		day = 36524;
	} else {
		year += day / 36524;
		day %= 36524;
	}
	year *= 25;
	year += day / 1461;
	day %= 1461;
	year *= 4;
	if (day == 1460) {
		year += 3;
		day = 365;
	} else {
		year += day / 365;
		day %= 365;
	}
	day *= 10;
	if ((day + 5) / 306 >= 10)
		++year;
	currentyear = year;
	flagneedcurrentyear = 0;
}

/* UNIX ls does not show the year for dates in the last six months. */
/* So we have to guess the year. */
/* Apparently NetWare uses ``twelve months'' instead of ``six months''; ugh. */
/* Some versions of ls also fail to show the year for future dates. */
static long
guesstai(long month, long mday)
{
	long year;
	long t = 0;

	initnow();

	for (year = currentyear - 1; year < currentyear + 100; ++year) {
		t = totai(year, month, mday);
		if (now - t < 350 * 86400)
			break;
	}

	return t;
}

static inline int
getmonth(const unsigned char *buf, int len)
{
	return (len == 3) ? month2num(buf) : -1;
}

static long
getlong(unsigned char *buf, int len)
{
	long u = 0;

	while (len-- > 0)
		u = u * 10 + (*buf++ - '0');
	return u;
}

int
ftpparse(struct ftpparse *fp, unsigned char *buf, int len)
{
	int i;
	int j;
	int state;
	long size = 0;
	long year;
	long month = 0;
	long mday = 0;
	long hour;
	long minute;

	memset(fp, 0, sizeof(*fp));

	if (len < 2)		/* an empty name in EPLF, with no info, could be 2 chars */
		return 0;

	switch (*buf) {
			/* see http://pobox.com/~djb/proto/eplf.txt */
			/* "+i8388621.29609,m824255902,/,\tdev" */
			/* "+i8388621.44468,m839956783,r,s10376,\tRFCEPLF" */
		case '+':
			i = 1;
			for (j = 1; j < len; ++j) {
				if (buf[j] == ASCII_TAB) {
					fp->name = buf + j + 1;
					fp->namelen = len - j - 1;
					return 1;
				}

				if (buf[j] != ',')
					continue;

				switch (buf[i]) {
					case '/':
						fp->flagtrycwd = 1;
						break;
					case 'r':
						fp->flagtryretr = 1;
						break;
					case 's':
						fp->sizetype = FTPPARSE_SIZE_BINARY;
						fp->size = getlong(buf + i + 1,
								   j - i - 1);
						break;
					case 'm':
						fp->mtimetype = FTPPARSE_MTIME_LOCAL;
						initbase();
						fp->mtime = base + getlong(buf + i + 1,
									   j - i - 1);
						break;
					case 'i':
						fp->idtype = FTPPARSE_ID_FULL;
						fp->id = buf + i + 1;
						fp->idlen = j - i - 1;
				}
				i = j + 1;
			}
			return 0;

		/* UNIX-style listing, without inum and without blocks */
		/* "-rw-r--r--   1 root     other        531 Jan 29 03:26 README" */
		/* "dr-xr-xr-x   2 root     other        512 Apr  8  1994 etc" */
		/* "dr-xr-xr-x   2 root     512 Apr  8  1994 etc" */
		/* "lrwxrwxrwx   1 root     other          7 Jan 25 00:17 bin -> usr/bin" */
		/* Also produced by Microsoft's FTP servers for Windows: */
		/* "----------   1 owner    group         1803128 Jul 10 10:18 ls-lR.Z" */
		/* "d---------   1 owner    group               0 May  9 19:45 Softlib" */
		/* Also WFTPD for MSDOS: */
		/* "-rwxrwxrwx   1 noone    nogroup      322 Aug 19  1996 message.ftp" */
		/* Also NetWare: */
		/* "d [R----F--] supervisor            512       Jan 16 18:53    login" */
		/* "- [R----F--] rhesus             214059       Oct 20 15:27    cx.exe" */
		/* Also NetPresenz for the Mac: */
		/* "-------r--         326  1391972  1392298 Nov 22  1995 MegaPhone.sit" */
		/* "drwxrwxr-x               folder        2 May 10  1996 network" */
		case 'b':
		case 'c':
		case 'd':
		case 'l':
		case 'p':
		case 's':
		case '-':

			if (*buf == 'd')
				fp->flagtrycwd = 1;
			if (*buf == '-')
				fp->flagtryretr = 1;
			if (*buf == 'l')
				fp->flagtrycwd = fp->flagtryretr = 1;

			if (buf[1] != ' ') {
				/* We wanna know permissions as well! And I
				 * decided to completely ignore the NetWare
				 * perms, they are very rare and of some
				 * nonstandart format. If you want them,
				 * though, I'll accept patch enabling them.
				 * --pasky */
				fp->perm = buf + 1;
				fp->permlen = strcspn(buf + 1, " ");
			}

			state = 1;
			i = 0;
			for (j = 1; j < len; ++j) {
				if ((buf[j] != ' ') || (buf[j - 1] == ' '))
					continue;

				switch (state) {
					case 1:	/* skipping perm */
						state = 2;
						break;
					case 2:	/* skipping nlink */
						state = 3;
						if ((j - i == 6) && (buf[i] == 'f'))	/* for NetPresenz */
							state = 4;
						break;
					case 3:	/* skipping uid */
						state = 4;
						break;
					case 4:	/* getting tentative size */
						size = getlong(buf + i, j - i);
						state = 5;
						break;
					case 5:	/* searching for month, otherwise getting tentative size */
						month = getmonth(buf + i, j - i);
						if (month >= 0)
							state = 6;
						else
							size = getlong(buf + i, j - i);
						break;
					case 6:	/* have size and month */
						mday = getlong(buf + i, j - i);
						state = 7;
						break;
					case 7:	/* have size, month, mday */
						if ((j - i == 4)
						    && (buf[i + 1] == ':')) {
							hour = getlong(buf + i, 1);
							minute = getlong(buf + i + 2, 2);
							fp->mtimetype = FTPPARSE_MTIME_REMOTEMINUTE;
							initbase();
							fp->mtime = base
								    + guesstai(month, mday)
								    + hour * 3600
								    + minute * 60;
						} else if ((j - i == 5)
							   && (buf[i + 2] == ':')) {
								hour = getlong(buf + i, 2);
								minute = getlong(buf + i + 3, 2);
								fp->mtimetype = FTPPARSE_MTIME_REMOTEMINUTE;
								initbase();
								fp->mtime = base
									    + guesstai(month, mday)
									    + hour * 3600
									    + minute * 60;
						} else if (j - i >= 4) {
							year = getlong(buf + i, j - i);
							fp->mtimetype = FTPPARSE_MTIME_REMOTEDAY;
							initbase();
							fp->mtime = base + totai(year, month, mday);
						} else
							return 0;

						fp->name = buf + j + 1;
						fp->namelen = len - j - 1;
						state = 8;
						break;
					case 8:	/* twiddling thumbs */
						break;
				}
				i = j + 1;
				while ((i < len) && (buf[i] == ' '))
					++i;
			}

			if (state != 8)
				return 0;

			fp->size = size;
			fp->sizetype = FTPPARSE_SIZE_BINARY;

			if (*buf == 'l')
				for (i = 0; i + 3 < fp->namelen; ++i)
					if (fp->name[i] == ' '
					    && fp->name[i + 1] == '-'
					    && fp->name[i + 2] == '>'
					    && fp->name[i + 3] == ' ') {
						fp->namelen = i;
						fp->symlink = &fp->name[i + 4];
						fp->symlinklen = len - (fp->name - buf)
							         - (fp->namelen + 4);
						break;
					}

			/* eliminate extra NetWare spaces */
			if ((buf[1] == ' ') || (buf[1] == '['))
				if (fp->namelen > 3)
					if (fp->name[0] == ' '
					    && fp->name[1] == ' '
					    && fp->name[2] == ' ') {
						fp->name += 3;
						fp->namelen -= 3;
					}

			return 1;
	}

	/* MultiNet (some spaces removed from examples) */
	/* "00README.TXT;1      2 30-DEC-1996 17:44 [SYSTEM] (RWED,RWED,RE,RE)" */
	/* "CORE.DIR;1          1  8-SEP-1996 16:09 [SYSTEM] (RWE,RWE,RE,RE)" */
	/* and non-MutliNet VMS: */
	/* "CII-MANUAL.TEX;1  213/216  29-JAN-1996 03:33:12  [ANONYMOU,ANONYMOUS]   (RWED,RWED,,)" */
	for (i = 0; i < len; ++i)
		if (buf[i] == ';')
			break;

	if (i < len) {
		fp->name = buf;
		fp->namelen = i;
		if (i > 4)
			if (buf[i - 4] == '.'
			    && buf[i - 3] == 'D'
			    && buf[i - 2] == 'I'
			    && buf[i - 1] == 'R') {
				fp->namelen -= 4;
				fp->flagtrycwd = 1;
			}
		if (!fp->flagtrycwd)
			fp->flagtryretr = 1;
		while (buf[i] != ' ')
			if (++i == len)
				return 0;
		while (buf[i] == ' ')
			if (++i == len)
				return 0;
		while (buf[i] != ' ')
			if (++i == len)
				return 0;
		while (buf[i] == ' ')
			if (++i == len)
				return 0;
		j = i;
		while (buf[j] != '-')
			if (++j == len)
				return 0;
		mday = getlong(buf + i, j - i);
		while (buf[j] == '-')
			if (++j == len)
				return 0;
		i = j;
		while (buf[j] != '-')
			if (++j == len)
				return 0;
		month = getmonth(buf + i, j - i);
		if (month < 0)
			return 0;
		while (buf[j] == '-')
			if (++j == len)
				return 0;
		i = j;
		while (buf[j] != ' ')
			if (++j == len)
				return 0;
		year = getlong(buf + i, j - i);
		while (buf[j] == ' ')
			if (++j == len)
				return 0;
		i = j;
		while (buf[j] != ':')
			if (++j == len)
				return 0;
		hour = getlong(buf + i, j - i);
		while (buf[j] == ':')
			if (++j == len)
				return 0;
		i = j;
		while ((buf[j] != ':') && (buf[j] != ' '))
			if (++j == len)
				return 0;
		minute = getlong(buf + i, j - i);

		fp->mtimetype = FTPPARSE_MTIME_REMOTEMINUTE;
		initbase();
		fp->mtime = base + totai(year, month, mday)
			    + hour * 3600 + minute * 60;

		return 1;
	}

	/* MSDOS format */
	/* 04-27-00  09:09PM       <DIR>          licensed */
	/* 07-18-00  10:16AM       <DIR>          pub */
	/* 04-14-00  03:47PM                  589 readme.htm */
	if (isdigit(*buf)) {
		i = 0;
		j = 0;
		while (buf[j] != '-')
			if (++j == len)
				return 0;
		month = getlong(buf + i, j - i) - 1;
		while (buf[j] == '-')
			if (++j == len)
				return 0;
		i = j;
		while (buf[j] != '-')
			if (++j == len)
				return 0;
		mday = getlong(buf + i, j - i);
		while (buf[j] == '-')
			if (++j == len)
				return 0;
		i = j;
		while (buf[j] != ' ')
			if (++j == len)
				return 0;
		year = getlong(buf + i, j - i);
		if (year < 50)
			year += 2000;
		if (year < 1000)
			year += 1900;
		while (buf[j] == ' ')
			if (++j == len)
				return 0;
		i = j;
		while (buf[j] != ':')
			if (++j == len)
				return 0;
		hour = getlong(buf + i, j - i);
		while (buf[j] == ':')
			if (++j == len)
				return 0;
		i = j;
		while ((buf[j] != 'A') && (buf[j] != 'P'))
			if (++j == len)
				return 0;
		minute = getlong(buf + i, j - i);
		if (hour == 12)
			hour = 0;
		if (buf[j] == 'A')
			if (++j == len)
				return 0;
		if (buf[j] == 'P') {
			hour += 12;
			if (++j == len)
				return 0;
		}
		if (buf[j] == 'M')
			if (++j == len)
				return 0;

		while (buf[j] == ' ')
			if (++j == len)
				return 0;
		if (buf[j] == '<') {
			fp->flagtrycwd = 1;
			while (buf[j] != ' ')
				if (++j == len)
					return 0;
		} else {
			i = j;
			while (buf[j] != ' ')
				if (++j == len)
					return 0;
			fp->size = getlong(buf + i, j - i);
			fp->sizetype = FTPPARSE_SIZE_BINARY;
			fp->flagtryretr = 1;
		}
		while (buf[j] == ' ')
			if (++j == len)
				return 0;

		fp->name = buf + j;
		fp->namelen = len - j;

		fp->mtimetype = FTPPARSE_MTIME_REMOTEMINUTE;
		initbase();
		fp->mtime = base + totai(year, month, mday)
			    + hour * 3600 + minute * 60;

		return 1;
	}

	/* Some useless lines, safely ignored: */
	/* "Total of 11 Files, 10966 Blocks." (VMS) */
	/* "total 14786" (UNIX) */
	/* "DISK$ANONFTP:[ANONYMOUS]" (VMS) */
	/* "Directory DISK$PCSA:[ANONYM]" (VMS) */

	return 0;
}
