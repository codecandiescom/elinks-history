/* $Id: win32.h,v 1.2 2003/10/27 03:26:27 pasky Exp $ */

#ifndef EL__OSDEP_WIN32_H
#define EL__OSDEP_WIN32_H

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

struct terminal;

void open_in_new_win32(struct terminal *term, unsigned char *exe_name,
		       unsigned char *param);

#endif

#endif
