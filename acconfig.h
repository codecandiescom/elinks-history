
@BOTTOM@
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
#undef CONFIG_SSL

/* Define to 1 if we should use OpenSSL */
#undef CONFIG_OPENSSL

/* Define to 1 if we should use GNUTLS */
#undef CONFIG_GNUTLS

/* Define to 1 if we should include leak debugger and internal error checking facilites */
#undef CONFIG_DEBUG

/* Directory containing default config */
#undef CONFDIR

/* Directory containing locales */
#undef LOCALEDIR

/* Directory containing libraries */
#undef LIBDIR

/* Enable direct use of system allocation functions */
#undef CONFIG_FASTMEM

/* Force use of internal functions instead of those of system libc */
#undef CONFIG_OWN_LIBC

/* Define to 1 if we should include a generic backtrace printing infrastructure */
/* (you will still need some support from the libc) */
#undef CONFIG_BACKTRACE

/* Define to 1 to reduce binary size as far as possible. */
#undef CONFIG_SMALL

/* Define to 1 to enable support for SMB protocol (requires smbclient). */
#undef CONFIG_SMB


#include "../feature.h"
