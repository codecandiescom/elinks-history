/* $Id: sysinfo.h,v 1.6 2004/08/14 23:48:41 jonas Exp $ */

#ifndef EL__OSDEP_UNIX_SYSINFO_H
#define EL__OSDEP_UNIX_SYSINFO_H

#ifdef CONFIG_UNIX

#define SYSTEM_NAME	"Unix"
#define SYSTEM_STR	"unix"
#define DEFAULT_SHELL	"/bin/sh"
#define GETSHELL	getenv("SHELL")

static inline int dir_sep(char x) { return x == '/'; }

#define FS_UNIX_RIGHTS
#define FS_UNIX_HARDLINKS
#define FS_UNIX_SOFTLINKS
#define FS_UNIX_USERS

#include <pwd.h>
#include <grp.h>

#ifdef HAVE_SYS_UN_H
#define USE_AF_UNIX
#else
#define DONT_USE_AF_UNIX
#endif
#define ASSOC_BLOCK
#define ASSOC_CONS_XWIN

#endif

#endif
