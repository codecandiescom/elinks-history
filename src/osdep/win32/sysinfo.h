/* $Id: sysinfo.h,v 1.2 2003/10/28 00:23:54 pasky Exp $ */

#ifndef EL__OSDEP_WIN32_SYSINFO_H
#define EL__OSDEP_WIN32_SYSINFO_H

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

#endif
