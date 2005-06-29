/* Environment variables handling */
/* $Id: env.c,v 1.1 2005/06/29 09:44:49 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "util/env.h"
#include "util/memory.h"
#include "util/string.h"

/* Set @name environment variable to @value or a substring of it:
 * On success, it returns 0.
 * If @value is NULL and on error, it returns -1.
 * If @length >= 0 and smaller than true @value length, it will
 * set @name to specified substring of @value.
 */
int
env_set(unsigned char *name, unsigned char *value, int length)
{
	int ret, true_length, allocated = 0;

	if (!value) return -1;

	true_length = strlen(value);
	if (length >= 0 && length < true_length) {
		/* Copy the substring. */
		value = memacpy(value, length);
		if (!value) return -1;
		allocated = 1;
	}

#ifdef HAVE_SETENV
	ret = setenv(name, value, 1);
#else
	/* XXX: what to do ?? */
	ret = -1;
#endif

	if (allocated) mem_free(value);
	return ret;
}
