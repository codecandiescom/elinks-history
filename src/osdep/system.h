/* $Id: system.h,v 1.3 2003/10/27 02:30:40 pasky Exp $ */

#ifndef EL__OSDEP_SYSTEM_H
#define EL__OSDEP_SYSTEM_H


/* System-type identifier */
#define SYS_UNIX	1
#define SYS_OS2		2
#define SYS_WIN32	3
#define SYS_BEOS	4
#define SYS_RISCOS	5

#undef UNIX
#undef OS2
#undef WIN32
#undef BEOS
#undef RISCOS

#if defined(__EMX__)
# define OS2
#elif defined(_WIN32)
# define WIN32
#elif defined(__BEOS__)
# define BEOS
#elif defined(__riscos__)
# define RISCOS
#else
# define UNIX
#endif

#ifdef UNIX

static inline int dir_sep(char x) { return x == '/'; }
#define FS_UNIX_RIGHTS
#define FS_UNIX_HARDLINKS
#define FS_UNIX_SOFTLINKS
#define FS_UNIX_USERS
#include <pwd.h>
#include <grp.h>
#define SYSTEM_ID SYS_UNIX
#define SYSTEM_NAME "Unix"
#define SYSTEM_STR "unix"
#define DEFAULT_SHELL "/bin/sh"
#define GETSHELL getenv("SHELL")
#ifdef HAVE_SYS_UN_H
#define USE_AF_UNIX
#else
#define DONT_USE_AF_UNIX
#endif
#define ASSOC_BLOCK
#define ASSOC_CONS_XWIN

#endif

#ifdef OS2

static inline int dir_sep(char x) { return x == '/' || x == '\\'; }
/*#define NO_ASYNC_LOOKUP*/
#define SYSTEM_ID SYS_OS2
#define SYSTEM_NAME "OS/2"
#define SYSTEM_STR "os2"
#define DEFAULT_SHELL "cmd.exe"
#define GETSHELL getenv("COMSPEC")
#define NO_FG_EXEC
#define DOS_FS
#define NO_FILE_SECURITY
#define NO_FORK_ON_EXIT
#define ASSOC_CONS_XWIN

#ifdef __EMX__
#define strcasecmp stricmp
#define strncasecmp strnicmp
#ifndef HAVE_STRCASECMP
#define HAVE_STRCASECMP
#endif
#ifndef HAVE_STRNCASECMP
#define HAVE_STRNCASECMP
#endif
#define read _read
#define write _write
#ifdef O_SIZE
#define USE_OPEN_PREALLOC
#endif
#endif

#endif

#ifdef WIN32

static inline int dir_sep(char x) { return x == '/' || x == '\\'; }
/*#define NO_ASYNC_LOOKUP*/
#define SYSTEM_ID SYS_WIN32
#define SYSTEM_NAME "Win32"
#define SYSTEM_STR "win32"
#define DEFAULT_SHELL "cmd.exe"
#define GETSHELL getenv("COMSPEC")
#define NO_FG_EXEC
#define DOS_FS
#define NO_FORK_ON_EXIT

#endif

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

#ifdef RISCOS

static inline int dir_sep(char x) { return x == '/' || x == '\\'; }
#define SYSTEM_ID SYS_RISCOS
#define SYSTEM_NAME "RISC OS"
#define SYSTEM_STR "riscos"
#define DEFAULT_SHELL "gos"
#define GETSHELL getenv("SHELL")
#define NO_FG_EXEC
#define NO_FILE_SECURITY
#define NO_FORK_ON_EXIT

#endif

#if !defined(HAVE_BEGINTHREAD) && !defined(BEOS)
#define THREAD_SAFE_LOOKUP
#endif

#if defined(HAVE_MOUOPEN) && !defined(USE_GPM) && defined(USE_MOUSE)
#define USING_OS2_MOUSE
#endif

#ifndef HAVE_SA_STORAGE
#define sockaddr_storage sockaddr
#endif

#endif
