/* $Id: unix.h,v 1.3 2003/10/27 03:29:08 pasky Exp $ */

#ifndef EL__OSDEP_UNIX_H
#define EL__OSDEP_UNIX_H

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
#if defined(HAVE_LIBGPM) && defined(HAVE_GPM_H)
#define GPM_MOUSE
#endif

#endif

#endif
