/* $Id: sysinfo.h,v 1.5 2004/08/14 23:34:20 jonas Exp $ */

#ifndef EL__OSDEP_BEOS_SYSINFO_H
#define EL__OSDEP_BEOS_SYSINFO_H

#ifdef CONFIG_BEOS

static inline int dir_sep(char x) { return x == '/'; }
#define FS_UNIX_RIGHTS
#define FS_UNIX_SOFTLINKS
#define FS_UNIX_USERS
#include <pwd.h>
#include <grp.h>
#define SYSTEM_NAME "BeOS"
#define SYSTEM_STR "beos"
#define DEFAULT_SHELL "/bin/sh"
#define GETSHELL getenv("SHELL")
#define NO_FORK_ON_EXIT
#define ASSOC_BLOCK
#define THREAD_SAFE_LOOKUP

#endif

#endif
