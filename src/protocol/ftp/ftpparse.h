/* FTP directory parsing */
/* $Id: ftpparse.h,v 1.9 2005/03/27 02:19:17 jonas Exp $ */

#ifndef EL__PROTOCOL_FTP_FTPPARSE_H
#define EL__PROTOCOL_FTP_FTPPARSE_H

struct ftpparse {
	unsigned char *name;	/* not necessarily 0-terminated */
	int namelen;
	int flagtrycwd;		/* 0 if cwd is definitely pointless, 1 otherwise */
	int flagtryretr;	/* 0 if retr is definitely pointless, 1 otherwise */
	int sizetype;
	long size;		/* number of octets */
	int mtimetype;
	time_t mtime;		/* modification time */
	int idtype;
	unsigned char *id;	/* not necessarily 0-terminated */
	int idlen;
	unsigned char *symlink;
	int symlinklen;
	unsigned char *perm;
	int permlen;
	int vms;
};

#define FTPPARSE_SIZE_UNKNOWN 0	/* Must be zero to permit memset() on struct ftpparse. */
#define FTPPARSE_SIZE_BINARY 1	/* size is the number of octets in TYPE I */
#define FTPPARSE_SIZE_ASCII 2	/* size is the number of octets in TYPE A */

/* When a time zone is unknown, it is assumed to be GMT. You may want
 * to use localtime() for LOCAL times, along with an indication that the
 * time is correct in the local time zone, and gmtime() for REMOTE* times.
 */
#define FTPPARSE_MTIME_UNKNOWN 0 	/* Must be zero to permit memset() on struct ftpparse. */
#define FTPPARSE_MTIME_LOCAL 1		/* time is correct */
#define FTPPARSE_MTIME_REMOTEMINUTE 2	/* time zone and secs are unknown */
#define FTPPARSE_MTIME_REMOTEDAY 3	/* time zone and time of day are unknown */

#define FTPPARSE_ID_UNKNOWN 0	/* Must be zero to permit memset() on struct ftpparse. */
#define FTPPARSE_ID_FULL 1	/* unique identifier for files on this FTP server */

/* ftpparse(struct ftpparse *fp, unsigned char *buf, int len) tries to parse one line
 * of LIST output.
 *
 * The line is an array of @len characters stored in @buf.
 * It should not include the terminating CR LF; so @buf[@len] is typically CR.
 *
 * If ftpparse() can't find a filename, it returns 0.
 *
 * If ftpparse() can find a filename, it fills in @fp and returns 1.
 *
 * The name is an array of @fp.namelen characters stored in @fp.name;
 * @fp.name points somewhere within @buf.
 */
int ftpparse(struct ftpparse *, unsigned char *, int);

#endif
