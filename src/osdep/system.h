/* $Id: system.h,v 1.17 2004/08/14 23:23:46 jonas Exp $ */

#ifndef EL__OSDEP_SYSTEM_H
#define EL__OSDEP_SYSTEM_H


/* System-type identifier */
#define SYS_UNIX	1
#define SYS_OS2		2
#define SYS_WIN32	3
#define SYS_BEOS	4
#define SYS_RISCOS	5

/* FIXME: Remove all usage of defines below and only use CONFIG_<OS> one */

#undef WIN32

#if defined(CONFIG_WIN32)
# define WIN32
#endif

#if !defined(CONFIG_BEOS) \
     && !defined(CONFIG_OS2) \
     && !defined(CONFIG_RISCOS) \
     && !defined(CONFIG_UNIX) \
     && !defined(CONFIG_WIN32)
#warning No OS platform defined, maybe config.h was not included
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
