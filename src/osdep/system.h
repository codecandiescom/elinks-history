/* $Id: system.h,v 1.11 2003/10/28 00:23:52 pasky Exp $ */

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

#include "osdep/beos/overrides.h"

#include "osdep/beos/sysinfo.h"
#include "osdep/os2/sysinfo.h"
#include "osdep/riscos/sysinfo.h"
#include "osdep/unix/sysinfo.h"
#include "osdep/win32/sysinfo.h"

#ifndef HAVE_SA_STORAGE
#define sockaddr_storage sockaddr
#endif

#endif
