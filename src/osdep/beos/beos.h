/* $Id: beos.h,v 1.2 2003/10/27 02:41:55 pasky Exp $ */

#ifndef EL__OSDEP_BEOS_H
#define EL__OSDEP_BEOS_H

#ifdef BEOS

static inline int dir_sep(char x) { return x == '/'; }
#define FS_UNIX_RIGHTS
#define FS_UNIX_SOFTLINKS
#define FS_UNIX_USERS
#include <pwd.h>
#include <grp.h>
#define SYSTEM_ID SYS_BEOS
#define SYSTEM_NAME "BeOS"
#define SYSTEM_STR "beos"
#define DEFAULT_SHELL "/bin/sh"
#define GETSHELL getenv("SHELL")
#define NO_FORK_ON_EXIT
#define ASSOC_BLOCK
#define THREAD_SAFE_LOOKUP

#include <sys/time.h>
#include <sys/types.h>
#include <net/socket.h>

int be_socket(int, int, int);
int be_connect(int, struct sockaddr *, int);
int be_getpeername(int, struct sockaddr *, int *);
int be_getsockname(int, struct sockaddr *, int *);
int be_listen(int, int);
int be_accept(int, struct sockaddr *, int *);
int be_bind(int, struct sockaddr *, int);
int be_pipe(int *);
int be_read(int, void *, int);
int be_write(int, void *, int);
int be_close(int);
int be_select(int, struct fd_set *, struct fd_set *, struct fd_set *, struct timeval *);
int be_getsockopt(int, int, int, void *, int *);

#endif

#endif
