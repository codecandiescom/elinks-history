
@BOTTOM@
/* Define to 1 if we have __va_copy() */
#undef HAVE_VA_COPY

/* Define to 1 if we have C99 compliant vsnprintf() */
#undef HAVE_C99_VSNPRINTF

/* Define to 1 if we have _beginthread() */
#undef HAVE_BEGINTHREAD

/* Define to 1 if we have MouOpen() */
#undef HAVE_MOUOPEN

/* Define to 1 if we have _read_kbd() */
#undef HAVE_READ_KBD

/* Define to 1 if we have clone() */
#undef HAVE_CLONE

/* Define to 1 if we should use pthreads */
#undef HAVE_PTHREADS

/* Define to 1 if we should use scripting */
#undef HAVE_SCRIPTING

/* Define to 1 if we should use Guile */
#undef HAVE_GUILE

/* Define to 1 if we should use Perl */
#undef HAVE_PERL

/* Define to 1 if we should use Lua */
#undef HAVE_LUA

/* Define to 1 if we should use SSL */
#undef HAVE_SSL

/* Define to 1 if we should use OpenSSL */
#undef HAVE_OPENSSL

/* Define to 1 if we should use GNUTLS */
#undef HAVE_GNUTLS

/* Define to 1 if we should include IPv6 support */
#undef IPV6

/* Define to 1 if we have XFree under OS/2 */
#undef X2

/* Define to 1 if we have X11 */
#undef HAVE_X11

/* Define to 1 if we should include leak debugger and internal error checking facilites */
#undef CONFIG_DEBUG

/* Directory containing default config */
#undef CONFDIR

/* Directory containing locales */
#undef LOCALEDIR

/* Directory containing libraries */
#undef LIBDIR

/* Enable direct use of system allocation functions */
#undef FASTMEM

/* Force use of internal functions instead of those of system libc */
#undef USE_OWN_LIBC

/* Define to 1 if we should include a generic backtrace printing infrastructure */
/* (you will still need some support from the libc) */
#undef CONFIG_BACKTRACE

/* Define to 1 to reduce binary size as far as possible. */
#undef ELINKS_SMALL

/* Define to 1 to enable support for SMB protocol (requires smbclient). */
#undef CONFIG_SMB


#include "../feature.h"
