/* Parsing of FTP `ls' directory output. */
/* $Id: parse.c,v 1.1 2005/03/27 02:20:34 jonas Exp $ */

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

#include "protocol/ftp/ftpparse.h"
#include "protocol/ftp/parse.h"
#include "util/string.h"


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

struct ftp_file_info *
parse_ftp_file_info(struct ftp_file_info *info, unsigned char *src, int len)
{
	struct ftpparse ftpparse_info;

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
