/* Parsing of FTP `ls' directory output. */
/* $Id: parse.c,v 1.9 2005/03/27 14:50:31 jonas Exp $ */

/* Parts of this file was part of GNU Wget
 * Copyright (C) 1995, 1996, 1997, 2000, 2001 Free Software Foundation, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>

#include "elinks.h"

#include "osdep/ascii.h"
#include "protocol/ftp/ftpparse.h"
#include "protocol/ftp/parse.h"
#include "util/conv.h"
#include "util/string.h"
#include "util/ttime.h"


#define skip_space_end(src, end) \
	do { while ((src) < (end) && isspace(*(src))) (src)++; } while (0)

#define skip_nonspace_end(src, end) \
	do { while ((src) < (end) && !isspace(*(src))) (src)++; } while (0)

static long
parse_ftp_number(unsigned char **src, unsigned char *end, long from, long to)
{
	long number = 0;
	unsigned char *pos = *src;

	for (; pos < end && isdigit(*pos); pos++)
		number = (*pos - '0') + 10 * number;

	*src = pos;

	if (number < from || to < number)
		return -1;

	return number;
}


/* Parser for the EPLF format (see http://pobox.com/~djb/proto/eplf.txt).
 *
 * Some example EPLF response, with the filename separator (tab) displayed as a
 * space:
 *
 * +i8388621.48594,m825718503,r,s280, djb.html
 * +i8388621.50690,m824255907,/, 514
 * +i8388621.48598,m824253270,r,s612, 514.html
 *
 * Lines end with \015\012 (CR-LF), but that is handled elsewhere.
 */

enum ftp_eplf {
	FTP_EPLF_FILENAME	= ASCII_TAB,	/* Filename follows */
	FTP_EPLF_PLAINFILE	= 'r',		/* RETR is possible */
	FTP_EPLF_DIRECTORY	= '/',		/* CWD is possible */
	FTP_EPLF_SIZE		= 's',		/* File size follows */
	FTP_EPLF_MTIME		= 'm',		/* Modification time follows */
	FTP_EPLF_ID		= 'i',		/* Unique file id follows */
};

static struct ftp_file_info *
parse_ftp_eplf_response(struct ftp_file_info *info, unsigned char *src, int len)
{
	/* Skip the '+'-char which starts the line. */
	unsigned char *end = src + len;
	unsigned char *pos = src++;

	/* Handle the series of facts about the file. */

	for (; src < end && pos; src = pos + 1) {
		/* Find the end of the current fact. */
		pos = memchr(src, ',', end - src);
		if (pos) *pos = '\0';

		switch (*src++) {
		case FTP_EPLF_FILENAME:
			if (src >= end) break;
			info->name.source = src;
			info->name.length = end - src;
			return info;

		case FTP_EPLF_DIRECTORY:
			info->type = FTP_FILE_DIRECTORY;
			break;

		case FTP_EPLF_PLAINFILE:
			info->type = FTP_FILE_PLAINFILE;
			break;

		case FTP_EPLF_SIZE:
			if (src >= pos) break;
			info->size = parse_ftp_number(&src, pos, 0, LONG_MAX);
			break;

		case FTP_EPLF_MTIME:
			if (src >= pos) break;
			info->mtime = (time_t) parse_ftp_number(&src, pos, 0, LONG_MAX);
			break;
		case FTP_EPLF_ID:
			/* Not used */
			break;
		}
	}

	return NULL;
}


/* Parser for UNIX-style listing, without inum and without blocks:
 * "-rw-r--r--   1 root     other        531 Jan 29 03:26 README"
 * "dr-xr-xr-x   2 root     other        512 Apr  8  1994 etc"
 * "dr-xr-xr-x   2 root     512 Apr  8  1994 etc"
 * "lrwxrwxrwx   1 root     other          7 Jan 25 00:17 bin -> usr/bin"
 *
 * Also produced by Microsoft's FTP servers for Windows:
 * "----------   1 owner    group         1803128 Jul 10 10:18 ls-lR.Z"
 * "d---------   1 owner    group               0 May  9 19:45 Softlib"
 *
 * Also WFTPD for MSDOS:
 * "-rwxrwxrwx   1 noone    nogroup      322 Aug 19  1996 message.ftp"
 *
 * Also NetWare:
 * "d [R----F--] supervisor            512       Jan 16 18:53    login"
 * "- [R----F--] rhesus             214059       Oct 20 15:27    cx.exe"
*
 * Also NetPresenz for the Mac:
 * "-------r--         326  1391972  1392298 Nov 22  1995 MegaPhone.sit"
 * "drwxrwxr-x               folder        2 May 10  1996 network"
 */

enum ftp_unix {
	FTP_UNIX_PERMISSIONS,
	FTP_UNIX_SIZE,
	FTP_UNIX_DAY,
	FTP_UNIX_TIME,
	FTP_UNIX_NAME
};

/* Converts Un*x-style symbolic permissions to number-style ones, e.g. string
 * rwxr-xr-x to 755. For now, it knows nothing of setuid/setgid/sticky. ACLs are
 * ignored. */
static int
parse_ftp_unix_permissions(const unsigned char *src, int len)
{
	int perms = 0, i;

	if (len < 9)
		return 0;

	for (i = 0; i < 3; i++, src += 3) {
		perms <<= 3;
		perms += (((src[0] == 'r') << 2)
		      +   ((src[1] == 'w') << 1)
		      +    (src[2] == 'x'
			 || src[2] == 's'));
	}

	return perms;
}

static struct ftp_file_info *
parse_ftp_unix_response(struct ftp_file_info *info, unsigned char *src, int len)
{
	unsigned char *end = src + len;
	enum ftp_file_type type = *src++;
	unsigned char *pos = src;
	struct tm mtime;
	enum ftp_unix fact;

	/* Decide the file type. */
	switch (type) {
	case FTP_FILE_PLAINFILE:
	case FTP_FILE_DIRECTORY:
	case FTP_FILE_SYMLINK:
		info->type = type;
		break;

	default:
		info->type = FTP_FILE_UNKNOWN;
	}

	memset(&mtime, 0, sizeof(mtime));
	mtime.tm_isdst = -1;

	skip_space_end(src, end);
	fact = FTP_UNIX_PERMISSIONS;

	for (pos = src; src < end; src = pos) {
		skip_nonspace_end(pos, end);

		switch (fact) {
		case FTP_UNIX_PERMISSIONS:
			/* We wanna know permissions as well! And I decided to
			 * completely ignore the NetWare perms, they are very
			 * rare and of some nonstandart format.  If you want
			 * them, though, I'll accept patch enabling them.
			 * --pasky */
			if (pos - src == 9)
				info->permissions = parse_ftp_unix_permissions(src, 9);
			fact = FTP_UNIX_SIZE;
			break;

		case FTP_UNIX_SIZE:
			/* Search for the size and month name combo: */
			if (info->size != FTP_SIZE_UNKNOWN
			    && pos - src == 3) {
				int month = month2num(src);

				if (month == -1)
					break;

				fact = FTP_UNIX_DAY;
				mtime.tm_mon = month;
				break;
			}

			if (!isdigit(*src)) {
				info->size = FTP_SIZE_UNKNOWN;
				break;
			}

			info->size = parse_ftp_number(&src, pos, 0, LONG_MAX);
			break;

		case FTP_UNIX_DAY:
			mtime.tm_mday = parse_ftp_number(&src, pos, 1, 31);
			fact = FTP_UNIX_TIME;
			break;

		case FTP_UNIX_TIME:
			/* This ought to be either the time, or the
			 * year. Let's be flexible! */
			fact = FTP_UNIX_NAME;

			/* We must deal with digits.  */
			if (!isdigit (*src))
				break;

			mtime.tm_year = parse_ftp_number(&src, pos, 0, LONG_MAX);

			/* If we have a number x, it's a year. If we have x:y,
			 * it's hours and minutes. */
			if (src >= pos || *src != ':')
				break;

			src++;

			/* This means these were hours!  */
			mtime.tm_hour = mtime.tm_year;
			mtime.tm_min  = parse_ftp_number(&src, pos, 0, 59);
			mtime.tm_year = 0;

			/* If we have x:y:z, z are seconds.  */
			if (src >= pos || *src != ':')
				break;

			src++;
			mtime.tm_sec  = parse_ftp_number(&src, pos, 0, 59);
			break;

		case FTP_UNIX_NAME:
			/* Since the file name may contain spaces use @end as the
			 * token ending and not @pos. */

			info->name.source = src;
			info->name.length = end - src;

			/* Some FTP sites choose to have ls -F as their default
			 * LIST output, which marks the symlinks with a trailing
			 * `@', directory names with a trailing `/' and
			 * executables with a trailing `*'. This is no problem
			 * unless encountering a symbolic link ending with `@',
			 * or an executable ending with `*' on a server without
			 * default -F output. I believe these cases are very
			 * rare. */

#define check_trailing_char(string, trailchar) \
	((string)->source[(string)->length - 1] == (trailchar))

			switch (info->type) {
			case FTP_FILE_DIRECTORY:
				/* Check for trailing `/' */
				if (check_trailing_char(&info->name, '/'))
					info->name.length--;
				break;

			case FTP_FILE_SYMLINK:
				/* If the file is a symbolic link, it should
				 * have a ` -> ' somewhere. */
				while (pos && pos + 3 < end) {
					if (!memcmp(pos, " -> ", 4)) {
						info->symlink.source = pos + 4;
						info->symlink.length = end - pos - 4;
						info->name.length = pos - src;
						break;
					} else {
						pos = memchr(pos, ' ', end - pos);
					}
				}

				if (!info->symlink.source)
					return NULL;

				/* Check for trailing `@' on link and trailing
				 * `/' on the link target if it's a directory */
				if (check_trailing_char(&info->name, '@'))
					info->name.length--;

				if (check_trailing_char(&info->symlink, '/'))
					info->symlink.length--;
				break;

			case FTP_FILE_PLAINFILE:
				/* Check for trailing `*' on files which are
				 * executable. */
				if ((info->permissions & 0111)
				    && check_trailing_char(&info->name, '*'))
					info->name.length--;

			default:
				break;
			}

			if (mtime.tm_year == 0) {
				/* Get the current time.  */
				time_t timenow = time(NULL);
				struct tm *now = localtime(&timenow);

				/* Some listings will not specify the year if it
				 * is "obvious" that the file was from the
				 * previous year. E.g. if today is 97-01-12, and
				 * you see a file of Dec 15th, its year is 1996,
				 * not 1997. Thanks to Vladimir Volovich for
				 * mentioning this! */
				if (mtime.tm_mon > now->tm_mon)
					mtime.tm_year = now->tm_year - 1;
				else
					mtime.tm_year = now->tm_year;
			}

			if (mtime.tm_year >= 1900)
				mtime.tm_year -= 1900;

			info->mtime = mktime(&mtime); /* store the time-stamp */
			info->local_time_zone = 1;

			return info;
		}

		skip_space_end(pos, end);
	}

	return NULL;
}


/* Parser for VMS-style MultiNet (some spaces removed from examples):
 * "00README.TXT;1      2 30-DEC-1996 17:44 [SYSTEM] (RWED,RWED,RE,RE)"
 * "CORE.DIR;1          1  8-SEP-1996 16:09 [SYSTEM] (RWE,RWE,RE,RE)"
 *
 * And non-MutliNet VMS:
 * "CII-MANUAL.TEX;1  213/216  29-JAN-1996 03:33:12  [ANONYMOU,ANONYMOUS]   (RWED,RWED,,)"
 */

/* Converts VMS symbolic permissions to number-style ones, e.g. string
 * RWED,RWE,RE to 755. "D" (delete) is taken to be equal to "W" (write).
 * Inspired by a patch of Stoyan Lekov <lekov@eda.bg>. */
static int
parse_ftp_vms_permissions(const unsigned char *src, int len)
{
	int perms = 0;
	int pos;

	for (pos = 0; pos < len; pos++) {
		switch (src[pos]) {
		case ',': perms <<= 3; break;
		case 'R': perms  |= 4; break;
		case 'W': perms  |= 2; break;
		case 'D': perms  |= 2; break;
		case 'E': perms  |= 1; break;
		default:
			 /* Wrong VMS permissons! */
			  return 0;
		}
	}

	return perms;
}

static struct ftp_file_info *
parse_ftp_vms_response(struct ftp_file_info *info, unsigned char *src, int len)
{
	struct tm mtime;
	unsigned char *end = src + len;
	unsigned char *pos;

	/* First column: Name. A bit of black magic again. The name maybe either
	 * ABCD.EXT or ABCD.EXT;NUM and it might be on a separate line.
	 * Therefore we will first try to get the complete name until the first
	 * space character; if it fails, we assume that the name occupies the
	 * whole line. After that we search for the version separator ";", we
	 * remove it and check the extension of the file; extension .DIR denotes
	 * directory. */

	pos = memchr(src, ';', end - src);
	if (!pos) return NULL;

	info->name.source = src;
	info->name.length = pos - src;

	/* If the name ends on .DIR or .DIR;#, it's a directory. We also
	 * set the file size to zero as the listing does tell us only
	 * the size in filesystem blocks - for an integrity check (when
	 * mirroring, for example) we would need the size in bytes. */

	if (info->name.length > 4 && !memcmp(&pos[-4], ".DIR", 4)) {
		info->type  = FTP_FILE_DIRECTORY;
		info->name.length -= 4;
	} else {
		info->type  = FTP_FILE_PLAINFILE;
	}

	skip_nonspace_end(pos, end);
	skip_space_end(pos, end);
	src = pos;


	/* Second column, if exists, or the first column of the next line
	 * contain file size in blocks. We will skip it. */

	if (src >= end) {
		/* FIXME: Handle multi-lined views. */
		return NULL;
	}

	skip_nonspace_end(src, end);
	skip_space_end(src, end);
	if (src >= end) return NULL;


	/* Third/Second column: Date DD-MMM-YYYY. */

	memset(&mtime, 0, sizeof(mtime));
	mtime.tm_isdst = -1;

	mtime.tm_mday = parse_ftp_number(&src, end, 1, 31);
	/* If the server produces garbage like
	 * 'EA95_0PS.GZ;1      No privilege for attempted operation'
	 * the check for '-' after the day will fail. */
	if (src + 2 >= end || *src != '-')
		return NULL;

	src++;

	pos = memchr(src, '-', end - src);
	if (!pos) return NULL;

	if (pos - src == 3) {
		mtime.tm_mon = month2num(src);
	}

	/* Unknown months are mapped to January */
	if (mtime.tm_mon < 0)
		mtime.tm_mon = 0;

	pos++;
	mtime.tm_year = parse_ftp_number(&pos, end, 0, LONG_MAX) - 1900;

	skip_space_end(pos, end);
	if (pos >= end) return NULL;
	src = pos;


	/* Fourth/Third column: Time hh:mm[:ss] */

	mtime.tm_hour = parse_ftp_number(&src, end, 0, 23);
	if (src >= end || *src != ':')
		return NULL;

	src++;

	mtime.tm_min = parse_ftp_number(&src, end, 0, 59);
	if (src >= end) return NULL;

	if (*src == ':') {
		src++;
		mtime.tm_sec = parse_ftp_number(&src, end, 0, 59);
		if (src >= end) return NULL;
	}

	/* Store the time-stamp */
	info->mtime = mktime(&mtime);

	/* Be more tolerant from here on ... */


	/* Skip the fifth column */

	skip_space_end(src, end);
	skip_nonspace_end(src, end);
	skip_space_end(src, end);
	if (src >= end) return info;


	/* Sixth column: Permissions */

	src = memchr(src, '(', end - src);
	if (!src || src >= end)
		return info;

	src++;

	pos = memchr(src, ')', end - src);
	if (!pos) return info;

	/* Permissons have the format "RWED,RWED,RE" */
	info->permissions = parse_ftp_vms_permissions(src, pos - src);

	return info;
}


struct ftp_file_info *
parse_ftp_file_info(struct ftp_file_info *info, unsigned char *src, int len)
{
	struct ftpparse ftpparse_info;

	switch (*src) {
	case '+':
		return parse_ftp_eplf_response(info, src, len);

	case 'b':
	case 'c':
	case 'd':
	case 'l':
	case 'p':
	case 's':
	case '-':
		return parse_ftp_unix_response(info, src, len);

	default:
		if (memchr(src, ';', len))
			return parse_ftp_vms_response(info, src, len);
		break;
	}

	memset(&ftpparse_info, 0, sizeof(ftpparse_info));

	if (!ftpparse(&ftpparse_info, src, len))
		return NULL;

	if (ftpparse_info.flagtrycwd) {
		if (ftpparse_info.flagtryretr)
			info->type = FTP_FILE_SYMLINK;
		else
			info->type = FTP_FILE_DIRECTORY;
	} else {
		info->type = FTP_FILE_PLAINFILE;
	}

	if (ftpparse_info.perm && ftpparse_info.permlen) {
		unsigned char *perm = ftpparse_info.perm;
		int permlen = ftpparse_info.permlen;

		if (ftpparse_info.vms)
			info->permissions = parse_ftp_vms_permissions(perm, permlen);
		else
			info->permissions = parse_ftp_unix_permissions(perm, permlen);
	}	

	if (ftpparse_info.sizetype != FTPPARSE_SIZE_UNKNOWN)
		info->size = ftpparse_info.size;

	if (ftpparse_info.mtime > 0) {
		info->mtime = ftpparse_info.mtime;
		info->local_time_zone = (FTPPARSE_MTIME_LOCAL == ftpparse_info.mtimetype);
	}

	info->name.source = ftpparse_info.name;
	info->name.length = ftpparse_info.namelen;

	info->symlink.source = ftpparse_info.symlink;
	info->symlink.length = ftpparse_info.symlinklen;

	return info;
}
