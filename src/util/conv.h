/* $Id: conv.h,v 1.7 2003/05/12 20:37:46 pasky Exp $ */

#ifndef EL__UTIL_CONV_H
#define EL__UTIL_CONV_H


static inline unsigned char
upcase(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'z') ch -= 0x20;
	return ch;
}


long strtolx(unsigned char *, unsigned char **);

unsigned char hx(int);
int unhx(unsigned char);

/* These use granular allocation stuff like some others funtions
 * in util/string.c, we should perhaps group them. --Zas */
int add_num_to_str(unsigned char **str, int *len, long num);
int add_knum_to_str(unsigned char **str, int *len, long num);
void add_htmlesc_str(unsigned char **, int *, unsigned char *, int);


/* These are fast functions to convert integers to string, or to hexadecimal string. */

int elinks_ulongcat(unsigned char *s, unsigned int *slen, unsigned long number,
		    unsigned int width, unsigned char fillchar);
/* Type casting is enforced, to shorten calls. --Zas */
#define ulongcat(s, slen, number, width, fillchar) \
	elinks_ulongcat((unsigned char *) (s), \
			(unsigned int *) (slen), \
			(unsigned long) (number), \
			(unsigned int) (width), \
			(unsigned char) (fillchar))

int elinks_longcat(unsigned char *s, unsigned int *slen, long number,
		   unsigned int width, unsigned char fillchar);
/* Type casting is enforced, to shorten calls. --Zas */
#define longcat(s, slen, number, width, fillchar) \
	 elinks_longcat((unsigned char *) (s), \
			(unsigned int *) (slen), \
			(long) (number), \
			(unsigned int) (width), \
			(unsigned char) (fillchar))

int elinks_ulonghexcat(unsigned char *s, unsigned int *slen, unsigned long number,
		       unsigned int width, unsigned char fillchar, unsigned int upper);
/* Type casting is enforced, to shorten calls. --Zas */
#define ulonghexcat(s, slen, number, width, fillchar, upper) \
	elinks_ulonghexcat((unsigned char *) (s), \
			   (unsigned int *) (slen), \
			   (unsigned long) (number), \
			   (unsigned int) (width), \
			   (unsigned char) (fillchar), \
			   (unsigned int) (upper))


/* XXX: Compatibility only. Remove these at some time. --Zas */
#define snprint(str, len, num) ulongcat(str, NULL, num, len, 0);
#define snzprint(str, len, num) longcat(str, NULL, num, len, 0);

#endif
