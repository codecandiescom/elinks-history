/* $Id: sysinfo.h,v 1.5 2004/08/14 23:48:42 jonas Exp $ */

#ifndef EL__OSDEP_WIN32_SYSINFO_H
#define EL__OSDEP_WIN32_SYSINFO_H

#ifdef CONFIG_WIN32

#define SYSTEM_NAME	"Win32"
#define SYSTEM_STR	"win32"
#define DEFAULT_SHELL	"cmd.exe"
#define GETSHELL	getenv("COMSPEC")

static inline int dir_sep(char x) { return x == '/' || x == '\\'; }

/*#define NO_ASYNC_LOOKUP*/
#define NO_FG_EXEC
#define DOS_FS
#define NO_FORK_ON_EXIT

#endif

#endif
