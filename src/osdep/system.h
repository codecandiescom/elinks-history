/* $Id: system.h,v 1.5 2003/10/27 02:40:12 pasky Exp $ */

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

#include "osdep/unix.h"
#include "osdep/os2.h"
#include "osdep/win32.h"
#include "osdep/beos.h"
#include "osdep/riscos.h"

#if defined(HAVE_BEGINTHREAD) || defined(BEOS)
#define THREAD_SAFE_LOOKUP
#endif

#if defined(HAVE_MOUOPEN) && !defined(USE_GPM) && defined(USE_MOUSE)
#define USING_OS2_MOUSE
#endif

#ifndef HAVE_SA_STORAGE
#define sockaddr_storage sockaddr
#endif

#endif
