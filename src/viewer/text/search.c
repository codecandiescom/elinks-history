/* Searching in the HTML document */
/* $Id: search.c,v 1.123 2003/11/17 18:45:20 kuser Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h> /* tolower() */
#include <sys/types.h> /* FreeBSD needs this before regex.h */
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/group.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/link.h"
#include "viewer/text/search.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

/* FIXME: Add comments!! --Zas */

static inline void
add_srch_chr(struct document *document, unsigned char c, int x, int y, int nn)
{
	int n;

	assert(document);
	if_assert_failed return;

	n = document->nsearch;

	if (c == ' ' && (!n || document->search[n - 1].c == ' ')) return;
	document->search[n].c = c;
	document->search[n].x = x;
	document->search[n].y = y;
	document->search[n].n = nn;
	document->nsearch++;
}

static void
sort_srch(struct document *document)
{
	int i;
	int *min, *max;

	assert(document);
	if_assert_failed return;

	document->slines1 = mem_calloc(document->height, sizeof(struct search *));
	if (!document->slines1) return;

	document->slines2 = mem_calloc(document->height, sizeof(struct search *));
	if (!document->slines2) {
		mem_free(document->slines1);
		return;
	}

	min = mem_calloc(document->height, sizeof(int));
	if (!min) {
		mem_free(document->slines1);
		mem_free(document->slines2);
		return;
	}

	max = mem_calloc(document->height, sizeof(int));
	if (!max) {
		mem_free(document->slines1);
		mem_free(document->slines2);
		mem_free(min);
		return;
	}

	for (i = 0; i < document->height; i++) {
		min[i] = MAXINT;
		max[i] = 0;
	}

	for (i = 0; i < document->nsearch; i++) {
		struct search *s = &document->search[i];
		int sxn = s->x + s->n;

		if (s->x < min[s->y]) {
			min[s->y] = s->x;
		   	document->slines1[s->y] = s;
		}
		if (sxn > max[s->y]) {
			max[s->y] = sxn;
			document->slines2[s->y] = s;
		}
	}

	mem_free(min);
	mem_free(max);
}

static int
get_srch(struct document *document)
{
	struct node *node;
	int cnt = 0;
	int cc;

	assert(document);
	if_assert_failed return 0;

	cc = !document->search;

	foreachback (node, document->nodes) {
		register int x, y;
		int height = int_min(node->y + node->height, document->height);

#define ADD(cr, nn) do { \
	if (!cc) add_srch_chr(document, (cr), x, y, (nn)); \
	else cnt++; \
} while (0)

		for (y = node->y; y < height; y++) {
			int width = int_min(node->x + node->width,
					    document->data[y].l);

			for (x = node->x; x < width && document->data[y].d[x].data <= ' '; x++);

			for (; x < width; x++) {
				unsigned char c = document->data[y].d[x].data;

				if (c < ' ') c = ' ';

				if (c != ' ') {
					ADD(c, 1);
				} else {
					int xx;
					int count = 0;

					for (xx = x + 1; xx < width; xx++) {
						if (document->data[y].d[xx].data >= ' ') {
							count = xx - x;
							break;
						}
					}

					ADD(' ', count);
					x = xx - 1;
				}

			}

			ADD(' ', 0);
		}
#undef ADD

	}


	return cnt;
}

static void
get_search_data(struct document *document)
{
	int n;

	assert(document);
	if_assert_failed return;

	if (document->search) return;

	n = get_srch(document);
	if (!n) return;

	document->nsearch = 0;

	document->search = mem_alloc(n * sizeof(struct search));
	if (!document->search) return;

	get_srch(document);
	while (document->nsearch
	       && document->search[--document->nsearch].c == ' ');
	sort_srch(document);
}

static int
get_range(struct document *document, int y, int yw, int l,
	  struct search **s1, struct search **s2)
{
	register int i;

	assert(document && s1 && s2);
	if_assert_failed return -1;

	*s1 = *s2 = NULL;
	int_lower_bound(&y, 0);

	for (i = y; i < y + yw && i < document->height; i++) {
		if (document->slines1[i] && (!*s1 || document->slines1[i] < *s1))
			*s1 = document->slines1[i];
		if (document->slines2[i] && (!*s2 || document->slines2[i] > *s2))
			*s2 = document->slines2[i];
	}
	if (!*s1 || !*s2) return -1;

	*s1 -= l;

	if (*s1 < document->search)
		*s1 = document->search;
	if (*s2 > document->search + document->nsearch - l + 1)
		*s2 = document->search + document->nsearch - l + 1;
	if (*s1 > *s2)
		*s1 = *s2 = NULL;
	if (!*s1 || !*s2)
		return -1;

	return 0;
}

/* Returns an allocated string which is a lowered copy of passed one. */
static unsigned char *
lowered_string(unsigned char *s, int l)
{
	unsigned char *ret;
	int len = l;

	if (len < 0) len = strlen(s);

	ret = mem_calloc(1, len + 1);
	if (ret && len) {
		register int i = len;

		do {
			ret[i] = tolower(s[i]);
		} while (i--);
	}

	return ret;
}

#ifdef HAVE_REGEX_H
static int
is_in_range_regex(struct document *document, int y, int yy,
		  unsigned char *text, int l, int *min, int *max,
		  struct search *s1, struct search *s2)
{
	unsigned char *doc;
	unsigned char *doctmp;
	int doclen;
	int found = 0;
	int regex_flags = REG_NEWLINE;
	int regexec_flags = 0;
	register int i;
	int reg_err;
	regex_t regex;
	regmatch_t regmatch;
	int pos = 0;
	struct search *search_start = s1;
	unsigned char save_c;

	if (get_opt_int("document.browse.search.regex") == 2)
		regex_flags |= REG_EXTENDED;

	if (!get_opt_bool("document.browse.search.case"))
		regex_flags |= REG_ICASE;

	reg_err = regcomp(&regex, text, regex_flags);
	if (reg_err) {
		/* TODO: error message */
		regfree(&regex);
		return 0;
	}

	doclen = s2 - s1 + l;
	if (!doclen) {
		regfree(&regex);
		return 0;
	}
	doc = mem_alloc(sizeof(unsigned char) * (doclen + 1));
	if (!doc) {
		regfree(&regex);
		return 0;
	}

	for (i = 0; i < doclen; i++) {
		if (i > 0 && s1[i - 1].c == ' ' && s1[i - 1].y != s1[i].y) {
			doc[i - 1] = '\n';
		}
		doc[i] = s1[i].c;
	}
	doc[doclen] = 0;

	doctmp = doc;

find_next:
	while (pos < doclen && (search_start[pos].y < y - 1
				|| search_start[pos].y > yy)) pos++;
	doctmp = &doc[pos];
	s1 = &search_start[pos];
	while (pos < doclen && search_start[pos].y >= y - 1
			    && search_start[pos].y <= yy) pos++;
	save_c = doc[pos];
	doc[pos] = 0;

	while (*doctmp && !regexec(&regex, doctmp, 1, &regmatch, regexec_flags)) {
		regexec_flags = REG_NOTBOL;
		l = regmatch.rm_eo - regmatch.rm_so;
		s1 += regmatch.rm_so;
		doctmp += regmatch.rm_so;

		if (s1[l].y < y || s1[l].y >= yy)
			goto next;

		found = 1;

		for (i = 0; i < l; i++) {
			if (!s1[i].n) continue;

			int_upper_bound(min, s1[i].x);
			int_lower_bound(max, s1[i].x + s1[i].n);
		}

next:
		doctmp += int_max(l, 1);
		s1 += int_max(l, 1);
	}

	doc[pos] = save_c;
	if (pos < doclen)
		goto find_next;

	regfree(&regex);
	mem_free(doc);

	return found;
}
#endif /* HAVE_REGEX_H */

static int
is_in_range_plain(struct document *document, int y, int yy,
		  unsigned char *text, int l, int *min, int *max,
		  struct search *s1, struct search *s2)
{
	unsigned char *txt;
	int found = 0;
	int case_sensitive = get_opt_int("document.browse.search.case");

	txt = case_sensitive ? stracpy(text) : lowered_string(text, l);
	if (!txt) return 0;

	/* TODO: This is a great candidate for nice optimizations. Fresh CS
	 * graduates can use their knowledge of ie. KMP (should be quite
	 * trivial, probably a starter; very fast as well) or Turbo-BM (or
	 * maybe some other Boyer-Moore variant, I don't feel that strong in
	 * this area), hmm?  >:) --pasky */

#define maybe_tolower(c) (case_sensitive ? (c) : tolower(c))

	for (; s1 <= s2; s1++) {
		register int i;

		if (maybe_tolower(s1->c) != txt[0]) {
srch_failed:
			continue;
		}

		for (i = 1; i < l; i++)
			if (maybe_tolower(s1[i].c) != txt[i])
				goto srch_failed;

		if (s1[i].y < y || s1[i].y >= yy)
			continue;

		found = 1;

		for (i = 0; i < l; i++) {
			if (!s1[i].n) continue;

			int_upper_bound(min, s1[i].x);
			int_lower_bound(max, s1[i].x + s1[i].n);
		}
	}

#undef maybe_tolower

	mem_free(txt);

	return found;
}

static int
is_in_range(struct document *document, int y, int yw,
	    unsigned char *text, int *min, int *max)
{
	struct search *s1, *s2;
	int l;

	assert(document && text && min && max);
	if_assert_failed return 0;

	*min = MAXINT, *max = 0;
	l = strlen(text);

	if (get_range(document, y, yw, l, &s1, &s2))
		return 0;

#ifdef HAVE_REGEX_H
	if (get_opt_int("document.browse.search.regex"))
		return is_in_range_regex(document, y, y + yw, text, l, min, max, s1, s2);
#endif
	return is_in_range_plain(document, y, y + yw, text, l, min, max, s1, s2);
}

#define realloc_points(pts, size) \
	mem_align_alloc(pts, size, (size) + 1, sizeof(struct point), 0xFF)

static void
get_searched_plain(struct document_view *doc_view, struct point **pt, int *pl,
		   int l, struct search *s1, struct search *s2)
{
	unsigned char *txt;
	struct point *points = NULL;
	int xmin, ymin;
	int xmax, ymax;
	int xoffset, yoffset;
	int len = 0;
	int case_sensitive = get_opt_int("document.browse.search.case");

	txt = case_sensitive ? stracpy(*doc_view->search_word)
			     : lowered_string(*doc_view->search_word, l);
	if (!txt) return;

	xmin = doc_view->x;
	ymin = doc_view->y;
	xmax = xmin + doc_view->width;
	ymax = ymin + doc_view->height;
	xoffset = xmin - doc_view->vs->x;
	yoffset = ymin - doc_view->vs->y;

#define maybe_tolower(c) (case_sensitive ? (c) : tolower(c))

	for (; s1 <= s2; s1++) {
		register int i;

		if (maybe_tolower(s1[0].c) != txt[0]) {
srch_failed:
			continue;
		}

		for (i = 1; i < l; i++)
			if (maybe_tolower(s1[i].c) != txt[i])
				goto srch_failed;

		for (i = 0; i < l; i++) {
			register int j;
			int y = s1[i].y + yoffset;

			if (y < ymin || y >= ymax)
				continue;

			for (j = 0; j < s1[i].n; j++) {
				int sx = s1[i].x + j;
				int x = sx + xoffset;

				if (x < xmin || x >= xmax)
					continue;

				if (!realloc_points(&points, len))
					continue;

				points[len].x = sx;
				points[len++].y = s1[i].y;
			}
		}
	}

#undef maybe_tolower

	mem_free(txt);
	*pt = points;
	*pl = len;
}

#ifdef HAVE_REGEX_H
static void
get_searched_regex(struct document_view *doc_view, struct point **pt, int *pl,
		   int l, struct search *s1, struct search *s2)
{
	unsigned char *doc;
	unsigned char *doctmp;
	int doclen;
	struct point *points = NULL;
	int xmin, ymin;
	int xmax, ymax;
	int xoffset, yoffset;
	int len = 0;
	int regex_flags = REG_NEWLINE;
	int regexec_flags = 0;
	int reg_err;
	register int i;
	regex_t regex;
	regmatch_t regmatch;
	int pos = 0;
	struct search *search_start = s1;
	unsigned char save_c;

	if (get_opt_int("document.browse.search.regex") == 2)
		regex_flags |= REG_EXTENDED;

	if (!get_opt_bool("document.browse.search.case"))
		regex_flags |= REG_ICASE;

	/* TODO: show error message */
	reg_err = regcomp(&regex, *doc_view->search_word, regex_flags);
	if (reg_err) {
#if 0
		/* Where and how should we display the error dialog ? */
		unsigned char regerror_string[MAX_STR_LEN];

		regerror(reg_err, &regex, regerror_string, sizeof(regerror_string));
#endif
		regfree(&regex);
		goto ret;
	}

	doclen = s2 - s1 + l;
	if (!doclen) {
		regfree(&regex);
		goto ret;
	}
	doc = mem_alloc(sizeof(unsigned char) * (doclen + 1));
	if (!doc) {
		regfree(&regex);
		goto ret;
	}

	for (i = 0; i < doclen; i++) {
		if (i > 0 && s1[i - 1].c == ' ' && s1[i - 1].y != s1[i].y) {
			doc[i - 1] = '\n';
		}
		doc[i] = s1[i].c;
	}
	doc[doclen] = 0;

	xmin = doc_view->x;
	ymin = doc_view->y;
	xmax = xmin + doc_view->width;
	ymax = ymin + doc_view->height;
	xoffset = xmin - doc_view->vs->x;
	yoffset = ymin - doc_view->vs->y;

	doctmp = doc;

find_next:
	while (pos < doclen && (search_start[pos].y + yoffset < ymin - 1
				|| search_start[pos].y + yoffset > ymax)) pos++;
	doctmp = &doc[pos];
	s1 = &search_start[pos];
	while (pos < doclen && search_start[pos].y + yoffset >= ymin - 1
			    && search_start[pos].y + yoffset <= ymax) pos++;
	save_c = doc[pos];
	doc[pos] = 0;

	while (*doctmp && !regexec(&regex, doctmp, 1, &regmatch, regexec_flags)) {
		regexec_flags = REG_NOTBOL;
		l = regmatch.rm_eo - regmatch.rm_so;
		s1 += regmatch.rm_so;
		doctmp += regmatch.rm_so;

		for (i = 0; i < l; i++) {
			register int j;
			int y = s1[i].y + yoffset;

			if (y < ymin || y >= ymax)
				continue;

			for (j = 0; j < s1[i].n; j++) {
				int sx = s1[i].x + j;
				int x = sx + xoffset;

				if (x < xmin || x >= xmax)
					continue;

				if (!realloc_points(&points, len))
					continue;

				points[len].x = sx;
				points[len++].y = s1[i].y;
			}
		}

		doctmp += int_max(l, 1);
		s1 += int_max(l, 1);
	}

	doc[pos] = save_c;
	if (pos < doclen)
		goto find_next;

	regfree(&regex);
	mem_free(doc);
ret:
	*pt = points;
	*pl = len;
}
#endif /* HAVE_REGEX_H */

static void
get_searched(struct document_view *doc_view, struct point **pt, int *pl)
{
	struct search *s1, *s2;
	int l;

	assert(doc_view && doc_view->vs && pt && pl);
	if_assert_failed return;

	if (!has_search_word(doc_view))
		return;

	get_search_data(doc_view->document);
	l = strlen(*doc_view->search_word);
	if (get_range(doc_view->document, doc_view->vs->y,
		      doc_view->height, l, &s1, &s2)
	   ) {
		*pt = NULL;
		*pl = 0;

		return;
	}

#ifdef HAVE_REGEX_H
	if (get_opt_int("document.browse.search.regex"))
		get_searched_regex(doc_view, pt, pl, l, s1, s2);
	else
#endif
		get_searched_plain(doc_view, pt, pl, l, s1, s2);
}

/* Highlighting of searched strings. */
void
draw_searched(struct terminal *term, struct document_view *doc_view)
{
	struct point *pt = NULL;
	int len = 0;

	assert(term && doc_view);
	if_assert_failed return;

	if (!has_search_word(doc_view))
		return;

	get_searched(doc_view, &pt, &len);
	if (len) {
		register int i;
		struct color_pair *color = get_bfu_color(term, "searched");
		int xoffset = doc_view->x - doc_view->vs->x;
		int yoffset = doc_view->y - doc_view->vs->y;

		for (i = 0; i < len; i++) {
			int x = pt[i].x + xoffset;
			int y = pt[i].y + yoffset;

			/* TODO: We should take in account original colors and
			 * combine them with defined color. */
#if 0
			/* This piece of code shows the old way of handling
			 * colors and screen char attributes. */
			unsigned co = get_char(term, x, y);
			co = ((co >> 3) & 0x0700) | ((co << 3) & 0x3800);
#endif

			draw_char_color(term, x, y, color);
		}
	}

	if (pt) mem_free(pt);
}



static void
search_for_do(struct session *ses, unsigned char *str, int direction)
{
	struct document_view *doc_view;

	assert(ses && str);
	if_assert_failed return;

	doc_view = current_frame(ses);

	assert(doc_view);
	if_assert_failed return;

	if (!*str) {
		if (ses->search_word) {
			mem_free(ses->search_word);
			ses->search_word = NULL;
		}
		if (ses->last_search_word) {
			mem_free(ses->last_search_word);
			ses->last_search_word = NULL;
		}
		return;
	}

	if (ses->search_word) mem_free(ses->search_word);
	ses->search_word = stracpy(str);
	if (!ses->search_word) return;

	if (ses->last_search_word) mem_free(ses->last_search_word);
	ses->last_search_word = stracpy(str);
	if (!ses->last_search_word) {
		mem_free(ses->search_word);
		return;
	}

	ses->search_direction = direction;
	find_next(ses, doc_view, 1);
}


void
search_for_back(struct session *ses, unsigned char *str)
{
	assert(ses && str);
	if_assert_failed return;

	search_for_do(ses, str, -1);
}

void
search_for(struct session *ses, unsigned char *str)
{
	assert(ses && str);
	if_assert_failed return;

	search_for_do(ses, str, 1);
}

static inline int
point_intersect(struct point *p1, int l1, struct point *p2, int l2)
{
#define HASH_SIZE	4096
#define HASH(p) ((((p).y << 6) + (p).x) & (HASH_SIZE - 1))

	register int i;
	static char hash[HASH_SIZE];
	static int first_time = 1;

	assert(p2);
	if_assert_failed return 0;

	if (first_time) memset(hash, 0, HASH_SIZE), first_time = 0;

	for (i = 0; i < l1; i++) hash[HASH(p1[i])] = 1;

	for (i = 0; i < l2; i++) {
		register int j;

		if (!hash[HASH(p2[i])]) continue;

		for (j = 0; j < l1; j++) {
			if (p1[j].x != p2[i].x) continue;
			if (p1[j].y != p2[i].y) continue;

			for (i = 0; i < l1; i++)
				hash[HASH(p1[i])] = 0;

			return 1;
		}
	}

	for (i = 0; i < l1; i++) hash[HASH(p1[i])] = 0;

	return 0;

#undef HASH
#undef HASH_SIZE
}

static int
find_next_link_in_search(struct document_view *doc_view, int d)
{
	struct point *pt = NULL;
	struct link *link;
	int len;

	assert(doc_view && doc_view->vs);
	if_assert_failed return 0;

	if (d == -2 || d == 2) {
		d /= 2;
		find_link(doc_view, d, 0);
		if (doc_view->vs->current_link == -1) return 1;
		goto nt;
	}

	while(doc_view->vs->current_link != -1
	      && next_in_view(doc_view, doc_view->vs->current_link + d, d, in_view, NULL)) {
nt:
		link = &doc_view->document->links[doc_view->vs->current_link];
		get_searched(doc_view, &pt, &len);
		if (point_intersect(pt, len, link->pos, link->n)) {
			mem_free(pt);
			return 0;
		}
		if (pt) mem_free(pt);
	}

	find_link(doc_view, d, 0);
	return 1;
}

void
find_next(struct session *ses, struct document_view *doc_view, int a)
{
	int p, min, max, c = 0;
	int step, hit_bottom = 0, hit_top = 0;
	int show_hit_top_bottom = get_opt_bool("document.browse.search.show_hit_top_bottom");

	assert(ses && ses->tab && ses->tab->term && doc_view && doc_view->vs);
	if_assert_failed return;

	p = doc_view->vs->y;
	step = ses->search_direction * doc_view->height;

	if (!a && ses->search_word) {
		if (!(find_next_link_in_search(doc_view, ses->search_direction))) return;
		p += step;
	}

	if (!ses->search_word) {
		if (!ses->last_search_word) {
			msg_box(ses->tab->term, NULL, 0,
				N_("Search"), AL_CENTER,
				N_("No previous search"),
				NULL, 1,
				N_("OK"), NULL, B_ENTER | B_ESC);
			return;
		}
		ses->search_word = stracpy(ses->last_search_word);
		if (!ses->search_word) return;
	}

	get_search_data(doc_view->document);

	do {
		if (is_in_range(doc_view->document, p, doc_view->height, ses->search_word, &min, &max)) {
			doc_view->vs->y = p;
			if (max >= min)
				doc_view->vs->x = int_min(int_max(doc_view->vs->x,
									  max - doc_view->width),
								  min);

			set_link(doc_view);
			find_next_link_in_search(doc_view, ses->search_direction * 2);

			if (!show_hit_top_bottom) return;
			if (hit_bottom)
				msg_box(ses->tab->term, NULL, 0,
					N_("Search"), AL_CENTER,
					N_("Search hit bottom, continuing at top."),
					NULL, 1,
					N_("OK"), NULL, B_ENTER | B_ESC);
			if (hit_top)
				msg_box(ses->tab->term, NULL, 0,
					N_("Search"), AL_CENTER,
					N_("Search hit top, continuing at bottom."),
					NULL, 1,
					N_("OK"), NULL, B_ENTER | B_ESC);
			return;
		}
		p += step;
		if (p > doc_view->document->height) {
			hit_bottom = 1;
			p = 0;
		}
		if (p < 0) {
			hit_top = 1;
			p = 0;
			while (p < doc_view->document->height) p += doc_view->height;
			p -= doc_view->height;
		}
		c += doc_view->height;
	} while (c < doc_view->document->height + doc_view->height);

	msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
		N_("Search"), AL_CENTER,
		msg_text(ses->tab->term, N_("Search string '%s' not found"),
			ses->search_word),
		ses, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

void
find_next_back(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && doc_view);
	if_assert_failed return;

	ses->search_direction = -ses->search_direction;
	find_next(ses, doc_view, a);
	ses->search_direction = -ses->search_direction;
}



static struct input_history search_history = {
	/* items: */	{ D_LIST_HEAD(search_history.entries) },
	/* size: */	0,
	/* dirty: */	0,
	/* nosave: */	0,
};

/* The dialog functions are clones of input_field() ones. Gross code
 * duplication. */
/* TODO: This is just hacked input_field(), containing a lot of generic crap
 * etc. The useless cruft should be blasted out. And it's quite ugly anyway,
 * a nice cleanup target ;-). --pasky */

struct search_dlg_hop {
	void *data;
	int whether_regex, cases;
};

static int
search_dlg_cancel(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	void (*fn)(void *) = widget_data->widget->udata;
	struct search_dlg_hop *hop = dlg_data->dlg->udata2;
	void *data = hop->data;

	if (fn) fn(data);
	cancel_dialog(dlg_data, widget_data);

	return 0;
}

static int
search_dlg_ok(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	void (*fn)(void *, unsigned char *) = widget_data->widget->udata;
	struct search_dlg_hop *hop = dlg_data->dlg->udata2;
	void *data = hop->data;
	unsigned char *text = dlg_data->widgets_data->cdata;

	update_dialog_data(dlg_data, widget_data);

	/* TODO: Some generic update_opt() or so. --pasky */

	{
		struct option *o = get_opt_rec(config_options,
					       "document.browse.search.regex");

		if (o->value.number != hop->whether_regex) {
			o->value.number = hop->whether_regex;
			o->flags |= OPT_TOUCHED;
		}
	}

	{
		struct option *o = get_opt_rec(config_options,
					       "document.browse.search.case");

		if (o->value.number != hop->cases) {
			o->value.number = hop->cases;
			o->flags |= OPT_TOUCHED;
		}
	}

	if (check_dialog(dlg_data)) return 1;

	add_to_input_history(dlg_data->dlg->widgets->info.field.history, text, 1);

	if (fn) fn(data, text);
	ok_dialog(dlg_data, widget_data);
	return 0;
}

/* XXX: @data is ignored. */
void
search_dlg_do(struct terminal *term, struct memory_list *ml, int intl,
	    unsigned char *title,
	    unsigned char *text,
	    unsigned char *okbutton,
	    unsigned char *cancelbutton,
	    void *data, struct input_history *history, int l,
	    unsigned char *def, int min, int max,
	    int (*check)(struct dialog_data *, struct widget_data *),
	    void (*fn)(void *, unsigned char *),
	    void (*cancelfn)(void *))
{
	struct dialog *dlg;
	unsigned char *field;
	struct search_dlg_hop *hop;

	if (intl) {
		title = _(title, term);
		text = _(text, term);
		okbutton = _(okbutton, term);
		cancelbutton = _(cancelbutton, term);
	}

	hop = mem_calloc(1, sizeof(struct search_dlg_hop));
	if (!hop) return;
	hop->whether_regex = get_opt_int("document.browse.search.regex");
	hop->cases = get_opt_int("document.browse.search.case");
	hop->data = data;

#define SEARCH_WIDGETS_COUNT 8
	dlg = calloc_dialog(SEARCH_WIDGETS_COUNT, l);
	if (!dlg) {
		mem_free(hop);
		return;
	}

	field = (unsigned char *) dlg + sizeof_dialog(SEARCH_WIDGETS_COUNT, 0);
	*field = 0;

	if (def) {
		int defsize = strlen(def) + 1;

		memcpy(field, def, (defsize > l) ? l - 1 : defsize);
	}

	dlg->title = title;
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.fit_datalen = 1;
	dlg->layout.float_groups = 1;
	dlg->udata = text;
	dlg->udata2 = hop;

	add_to_ml(&ml, hop, NULL);

	add_dlg_field(dlg, text, min, max, check, l, field, history);

	add_dlg_radio(dlg, _("Normal search", term), 1, 0, hop->whether_regex);
	add_dlg_radio(dlg, _("Regexp search", term), 1, 1, hop->whether_regex);
	add_dlg_radio(dlg, _("Extended regexp search", term), 1, 2, hop->whether_regex);
	add_dlg_radio(dlg, _("Case sensitive", term), 2, 1, hop->cases);
	add_dlg_radio(dlg, _("Case insensitive", term), 2, 0, hop->cases);

	add_dlg_button(dlg, B_ENTER, search_dlg_ok, okbutton, fn);
	add_dlg_button(dlg, B_ESC, search_dlg_cancel, cancelbutton, cancelfn);

	add_dlg_end(dlg, SEARCH_WIDGETS_COUNT);

	add_to_ml(&ml, dlg, NULL);
	do_dialog(term, dlg, ml);
}


void
search_back_dlg(struct session *ses, struct document_view *doc_view, int a)
{
	search_dlg_do(ses->tab->term, NULL, 1,
		    N_("Search backward"), N_("Search for text"),
		    N_("OK"), N_("Cancel"), ses, &search_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) search_for_back,
		    NULL);
}

void
search_dlg(struct session *ses, struct document_view *doc_view, int a)
{
	search_dlg_do(ses->tab->term, NULL, 1,
		    N_("Search"), N_("Search for text"),
		    N_("OK"), N_("Cancel"), ses, &search_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) search_for,
		    NULL);
}

void
init_search_history(void)
{
	load_input_history(&search_history, "searchhist");
}

void
done_search_history(void)
{
	save_input_history(&search_history, "searchhist");
	free_list(search_history.entries);
}
