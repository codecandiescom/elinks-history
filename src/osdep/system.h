/* $Id: system.h,v 1.13 2004/08/14 23:00:07 jonas Exp $ */

#ifndef EL__OSDEP_SYSTEM_H
#define EL__OSDEP_SYSTEM_H


/* System-type identifier */
#define SYS_UNIX	1
#define SYS_OS2		2
#define SYS_WIN32	3
#define SYS_BEOS	4
#define SYS_RISCOS	5

/* FIXME: Remove all usage of defines below and only use CONFIG_<OS> one */

#undef UNIX
#undef OS2
#undef WIN32
#undef BEOS

#if defined(CONFIG_OS2)
# define OS2
#elif defined(CONFIG_WIN32)
# define WIN32
#elif defined(CONFIG_BEOS)
# define BEOS
#elif defined(CONFIG_UNIX)
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
