/* Parsing of FTP `ls' directory output. */
/* $Id: parse.c,v 1.2 2005/03/27 04:12:06 jonas Exp $ */

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

#include "osdep/ascii.h"
#include "protocol/ftp/ftpparse.h"
#include "protocol/ftp/parse.h"
#include "util/conv.h"
#include "util/string.h"
#include "util/ttime.h"


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
			info->size = strtolx(src, &pos);
			break;

		case FTP_EPLF_MTIME:
			if (src >= pos) break;
			info->mtime = str_to_time_T(src);
			break;
		case FTP_EPLF_ID:
			/* Not used */
			break;
		}
	}

	return NULL;
}


struct ftp_file_info *
parse_ftp_file_info(struct ftp_file_info *info, unsigned char *src, int len)
{
	struct ftpparse ftpparse_info;

	if (*src == '+')
		return parse_ftp_eplf_response(info, src, len);

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
