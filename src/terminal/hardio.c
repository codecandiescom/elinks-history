/* Low-level terminal-suitable I/O routines */
/* $Id: hardio.c,v 1.3 2003/05/06 14:49:33 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "terminal/hardio.h"

int
hard_write(int fd, unsigned char *p, int l)
{
	int w = 1;
	int t = 0;

	while (l > 0 && w) {
		w = write(fd, p, l);
		if (w < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		t += w;
		p += w;
		l -= w;
	}

	return t;
}

int
hard_read(int fd, unsigned char *p, int l)
{
	int r = 1;
	int t = 0;

	while (l > 0 && r) {
		r = read(fd, p, l);
		if (r < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
#if 0 /* for debugging purpose */
		{
		 int ww;

		 for (ww = 0; ww < r; ww++)
			 fprintf(stderr, " %02x", (int) p[ww]);
		 fflush(stderr);
		}
#endif
	 	t += r;
		p += r;
		l -= r;
	}

	return t;
}
