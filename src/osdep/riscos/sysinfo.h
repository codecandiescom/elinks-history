/* $Id: sysinfo.h,v 1.2 2003/10/28 00:23:53 pasky Exp $ */

#ifndef EL__OSDEP_RISCOS_SYSINFO_H
#define EL__OSDEP_RISCOS_SYSINFO_H

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

#endif
