/* $Id: sysinfo.h,v 1.4 2004/08/14 23:19:00 jonas Exp $ */

#ifndef EL__OSDEP_OS2_SYSINFO_H
#define EL__OSDEP_OS2_SYSINFO_H

#ifdef CONFIG_OS2

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
#ifdef HAVE_BEGINTHREAD
#define THREAD_SAFE_LOOKUP
#endif
#if defined(HAVE_MOUOPEN) && defined(CONFIG_MOUSE)
#define OS2_MOUSE
#endif

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

#endif
