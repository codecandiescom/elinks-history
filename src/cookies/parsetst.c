/* Tool for testing of cookies string parser */
/* $Id: parsetst.c,v 1.4 2002/04/23 08:25:45 pasky Exp $ */

#if 0
/* Dunno.. --pasky */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cookies/parser.h>


int
main(int argc, char *argv[])
{
	struct cookie_str cstr;
	unsigned char name[1024], value[1024];

	printf("This thing is for testing of cookies name-value pair parser.\n"
	       "You probably do not want to mess with it :).\n");

	while (1) {
		printf("Enter string (empty==quit): "); fflush(stdout);

		cstr.str = malloc(1024);
		fgets(cstr.str, 1024, stdin);

		cstr.str[strlen(cstr.str) - 1] = '\0'; /* Strip newline. */
		if (!*cstr.str) return 0;

		if (!parse_cookie_str(&cstr)) {
			printf("ERROR while parsing '%s'!\n", cstr.str);
			continue;
		}

		memcpy(name, cstr.str, cstr.nam_end - cstr.str);
		name[cstr.nam_end - cstr.str] = '\0';
		memcpy(value, cstr.val_start, cstr.val_end - cstr.val_start);
		value[cstr.val_end - cstr.val_start] = '\0';

		printf("'%s' -> '%s' :: '%s'\n", cstr.str, name, value);
		free(cstr.str);
	}
}
