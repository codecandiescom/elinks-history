/* Searching in the HTML document */
/* $Id: search.c,v 1.285 2004/10/01 00:40:09 jonas Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h> /* tolower(), isprint() */
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
#include "config/kbdbind.h"
#include "document/document.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "sched/event.h"
#include "sched/session.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/link.h"
#include "viewer/text/search.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


#define SEARCH_HISTORY_FILENAME		"searchhist"

static INIT_INPUT_HISTORY(search_history);


static inline void
add_srch_chr(struct document *document, unsigned char c, int x, int y, int nn)
{
	assert(document);
	if_assert_failed return;

	if (c == ' ' && !document->nsearch) return;

	if (document->search) {
		int n = document->nsearch;

		if (c == ' ' && document->search[n - 1].c == ' ')
			return;

		document->search[n].c = c;
		document->search[n].x = x;
		document->search[n].y = y;
		document->search[n].n = nn;
	}

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
		min[i] = INT_MAX;
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

	assert(document && document->nsearch == 0);

	if_assert_failed return 0;

	foreachback (node, document->nodes) {
		int x, y;
		int height = int_min(node->box.y + node->box.height, document->height);

		for (y = node->box.y; y < height; y++) {
			int width = int_min(node->box.x + node->box.width,
					    document->data[y].length);

			for (x = node->box.x;
			     x < width && document->data[y].chars[x].data <= ' ';
			     x++);

			for (; x < width; x++) {
				unsigned char c = document->data[y].chars[x].data;
				int count = 0;
				int xx;

				if (document->data[y].chars[x].attr & SCREEN_ATTR_UNSEARCHABLE)
					continue;

				if (c > ' ') {
					add_srch_chr(document, c, x, y, 1);
					continue;
				}

				for (xx = x + 1; xx < width; xx++) {
					if (document->data[y].chars[xx].data < ' ')
						continue;
					count = xx - x;
					break;
				}

				add_srch_chr(document, ' ', x, y, count);
				x = xx - 1;
			}

			add_srch_chr(document, ' ', x, y, 0);
		}
	}

	return document->nsearch;
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

/* Returns -1 on assertion failure, 1 if s1 and s2 are not found,
 * and 0 if they are found. */
static int
get_range(struct document *document, int y, int height, int l,
	  struct search **s1, struct search **s2)
{
	int i;

	assert(document && s1 && s2);
	if_assert_failed return -1;

	*s1 = *s2 = NULL;
	int_lower_bound(&y, 0);

	for (i = y; i < y + height && i < document->height; i++) {
		if (document->slines1[i] && (!*s1 || document->slines1[i] < *s1))
			*s1 = document->slines1[i];
		if (document->slines2[i] && (!*s2 || document->slines2[i] > *s2))
			*s2 = document->slines2[i];
	}
	if (!*s1 || !*s2) return 1;

	*s1 -= l;

	if (*s1 < document->search)
		*s1 = document->search;
	if (*s2 > document->search + document->nsearch - l + 1)
		*s2 = document->search + document->nsearch - l + 1;
	if (*s1 > *s2)
		*s1 = *s2 = NULL;
	if (!*s1 || !*s2)
		return 1;

	return 0;
}

#ifdef HAVE_REGEX_H
static int
is_in_range_regex(struct document *document, int y, int height,
		  unsigned char *text, int textlen,
		  int *min, int *max,
		  struct search *s1, struct search *s2)
{
	int yy = y + height;
	unsigned char *doc;
	unsigned char *doctmp;
	int doclen;
	int found = 0;
	int regex_flags = REG_NEWLINE;
	int regexec_flags = 0;
	int i;
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
		return -2;
	}

	doclen = s2 - s1 + textlen;
	if (!doclen) {
		regfree(&regex);
		return 0;
	}
	doc = mem_alloc(sizeof(unsigned char) * (doclen + 1));
	if (!doc) {
		regfree(&regex);
		return -1;
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
		textlen = regmatch.rm_eo - regmatch.rm_so;
		s1 += regmatch.rm_so;
		doctmp += regmatch.rm_so;

		if (s1[textlen].y < y || s1[textlen].y >= yy)
			goto next;

		found = 1;

		for (i = 0; i < textlen; i++) {
			if (!s1[i].n) continue;

			int_upper_bound(min, s1[i].x);
			int_lower_bound(max, s1[i].x + s1[i].n);
		}

next:
		doctmp += int_max(textlen, 1);
		s1 += int_max(textlen, 1);
	}

	doc[pos] = save_c;
	if (pos < doclen)
		goto find_next;

	regfree(&regex);
	mem_free(doc);

	return found;
}
#endif /* HAVE_REGEX_H */

/* Returns an allocated string which is a lowered copy of passed one. */
static unsigned char *
lowered_string(unsigned char *text, int textlen)
{
	unsigned char *ret;

	if (textlen < 0) textlen = strlen(text);

	ret = mem_calloc(1, textlen + 1);
	if (ret && textlen) {
		do {
			ret[textlen] = tolower(text[textlen]);
		} while (textlen--);
	}

	return ret;
}

static int
is_in_range_plain(struct document *document, int y, int height,
		  unsigned char *text, int textlen,
		  int *min, int *max,
		  struct search *s1, struct search *s2)
{
	int yy = y + height;
	unsigned char *txt;
	int found = 0;
	int case_sensitive = get_opt_int("document.browse.search.case");

	txt = case_sensitive ? stracpy(text) : lowered_string(text, textlen);
	if (!txt) return -1;

	/* TODO: This is a great candidate for nice optimizations. Fresh CS
	 * graduates can use their knowledge of ie. KMP (should be quite
	 * trivial, probably a starter; very fast as well) or Turbo-BM (or
	 * maybe some other Boyer-Moore variant, I don't feel that strong in
	 * this area), hmm?  >:) --pasky */

#define maybe_tolower(c) (case_sensitive ? (c) : tolower(c))

	for (; s1 <= s2; s1++) {
		int i;

		if (maybe_tolower(s1->c) != txt[0]) {
srch_failed:
			continue;
		}

		for (i = 1; i < textlen; i++)
			if (maybe_tolower(s1[i].c) != txt[i])
				goto srch_failed;

		if (s1[i].y < y || s1[i].y >= yy)
			continue;

		found = 1;

		for (i = 0; i < textlen; i++) {
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
is_in_range(struct document *document, int y, int height,
	    unsigned char *text, int *min, int *max)
{
	struct search *s1, *s2;
	int textlen;

	assert(document && text && min && max);
	if_assert_failed return -1;

	*min = INT_MAX, *max = 0;
	textlen = strlen(text);

	if (get_range(document, y, height, textlen, &s1, &s2))
		return 0;

#ifdef HAVE_REGEX_H
	if (get_opt_int("document.browse.search.regex"))
		return is_in_range_regex(document, y, height, text, textlen,
					 min, max, s1, s2);
#endif
	return is_in_range_plain(document, y, height, text, textlen,
				 min, max, s1, s2);
}

#define realloc_points(pts, size) \
	mem_align_alloc(pts, size, (size) + 1, struct point, 0xFF)

static void
get_searched_plain(struct document_view *doc_view, struct point **pt, int *pl,
		   int l, struct search *s1, struct search *s2)
{
	unsigned char *txt;
	struct point *points = NULL;
	struct box *box;
	int xoffset, yoffset;
	int len = 0;
	int case_sensitive = get_opt_int("document.browse.search.case");

	txt = case_sensitive ? stracpy(*doc_view->search_word)
			     : lowered_string(*doc_view->search_word, l);
	if (!txt) return;

	box = &doc_view->box;
	xoffset = box->x - doc_view->vs->x;
	yoffset = box->y - doc_view->vs->y;

#define maybe_tolower(c) (case_sensitive ? (c) : tolower(c))

	for (; s1 <= s2; s1++) {
		int i;

		if (maybe_tolower(s1[0].c) != txt[0]) {
srch_failed:
			continue;
		}

		for (i = 1; i < l; i++)
			if (maybe_tolower(s1[i].c) != txt[i])
				goto srch_failed;

		for (i = 0; i < l; i++) {
			int j;
			int y = s1[i].y + yoffset;

			if (!row_is_in_box(box, y))
				continue;

			for (j = 0; j < s1[i].n; j++) {
				int sx = s1[i].x + j;
				int x = sx + xoffset;

				if (!col_is_in_box(box, x))
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
	int xoffset, yoffset;
	int len = 0;
	int regex_flags = REG_NEWLINE;
	int regexec_flags = 0;
	int reg_err;
	int i;
	regex_t regex;
	regmatch_t regmatch;
	int pos = 0;
	struct search *search_start = s1;
	unsigned char save_c;
	struct box *box;
	int y1, y2;

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

	box = &doc_view->box;
	xoffset = box->x - doc_view->vs->x;
	yoffset = box->y - doc_view->vs->y;
	y1 = doc_view->vs->y - 1;
	y2 = doc_view->vs->y + box->height;

	doctmp = doc;

find_next:
	while (pos < doclen) {
		int y = search_start[pos].y;

		if (y >= y1 && y <= y2) break;
		pos++;
	}
	doctmp = &doc[pos];
	s1 = &search_start[pos];

	while (pos < doclen) {
		int y = search_start[pos].y;

		if (y < y1 || y > y2) break;
		pos++;
	}
	save_c = doc[pos];
	doc[pos] = 0;

	while (*doctmp && !regexec(&regex, doctmp, 1, &regmatch, regexec_flags)) {
		regexec_flags = REG_NOTBOL;
		l = regmatch.rm_eo - regmatch.rm_so;
		s1 += regmatch.rm_so;
		doctmp += regmatch.rm_so;

		for (i = 0; i < l; i++) {
			int j;
			int y = s1[i].y + yoffset;

			if (!row_is_in_box(box, y))
				continue;

			for (j = 0; j < s1[i].n; j++) {
				int sx = s1[i].x + j;
				int x = sx + xoffset;

				if (!col_is_in_box(box, x))
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
		      doc_view->box.height, l, &s1, &s2)) {
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
		int i;
		struct color_pair *color = get_bfu_color(term, "searched");
		int xoffset = doc_view->box.x - doc_view->vs->x;
		int yoffset = doc_view->box.y - doc_view->vs->y;

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

	mem_free_if(pt);
}


enum find_error {
	FIND_ERROR_NONE,
	FIND_ERROR_NO_PREVIOUS_SEARCH,
	FIND_ERROR_HIT_TOP,
	FIND_ERROR_HIT_BOTTOM,
	FIND_ERROR_NOT_FOUND,
	FIND_ERROR_MEMORY,
	FIND_ERROR_REGEX,
};

static enum find_error find_next_do(struct session *ses,
				    struct document_view *doc_view,
				    int direction);

static void print_find_error(struct session *ses, enum find_error find_error);

static enum find_error
search_for_do(struct session *ses, unsigned char *str, int direction,
	      int report_errors)
{
	struct document_view *doc_view;
	enum find_error error;

	assert(ses && str);
	if_assert_failed return FIND_ERROR_NOT_FOUND;

	doc_view = current_frame(ses);

	assert(doc_view);
	if_assert_failed return FIND_ERROR_NOT_FOUND;

	mem_free_set(&ses->search_word, NULL);
	mem_free_set(&ses->last_search_word, NULL);

	if (!*str) return FIND_ERROR_NOT_FOUND;

	/* We only set the last search word because we don.t want find_next()
	 * to try to find next link in search before the search data has been
	 * initialized. find_next() will set ses->search_word for us. */
	ses->last_search_word = stracpy(str);
	if (!ses->last_search_word) return FIND_ERROR_NOT_FOUND;

	ses->search_direction = direction;

	error = find_next_do(ses, doc_view, 1);

	if (report_errors)
		print_find_error(ses, error);

	return error;
}

static void
search_for_back(struct session *ses, unsigned char *str)
{
	assert(ses && str);
	if_assert_failed return;

	search_for_do(ses, str, -1, 1);
}

static void
search_for(struct session *ses, unsigned char *str)
{
	assert(ses && str);
	if_assert_failed return;

	search_for_do(ses, str, 1, 1);
}


static inline int
point_intersect(struct point *p1, int l1, struct point *p2, int l2)
{
#define HASH_SIZE	4096
#define HASH(p) ((((p).y << 6) + (p).x) & (HASH_SIZE - 1))

	int i;
	static char hash[HASH_SIZE];
	static int first_time = 1;

	assert(p2);
	if_assert_failed return 0;

	if (first_time) memset(hash, 0, HASH_SIZE), first_time = 0;

	for (i = 0; i < l1; i++) hash[HASH(p1[i])] = 1;

	for (i = 0; i < l2; i++) {
		int j;

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
find_next_link_in_search(struct document_view *doc_view, int direction)
{
	assert(doc_view && doc_view->vs);
	if_assert_failed return 0;

	if (direction == -2 || direction == 2) {
		direction /= 2;
		if (direction < 0)
			find_link_page_up(doc_view);
		else
			find_link_page_down(doc_view);

		if (doc_view->vs->current_link == -1) return 1;
		goto nt;
	}

	while (doc_view->vs->current_link != -1
	       && next_link_in_view(doc_view, doc_view->vs->current_link + direction,
	                            direction, link_in_view, NULL)) {
		struct point *pt = NULL;
		struct link *link;
		int len;

nt:
		link = &doc_view->document->links[doc_view->vs->current_link];
		get_searched(doc_view, &pt, &len);
		if (point_intersect(pt, len, link->points, link->npoints)) {
			mem_free(pt);
			return 0;
		}
		mem_free_if(pt);
	}

	if (direction < 0)
		find_link_page_up(doc_view);
	else
		find_link_page_down(doc_view);

	return 1;
}

static enum find_error
find_next_do(struct session *ses, struct document_view *doc_view, int direction)
{
	int p, min, max, c = 0;
	int step, hit_bottom = 0, hit_top = 0;
	int height;

	assert(ses && ses->tab && ses->tab->term && doc_view && doc_view->vs
	       && direction);
	if_assert_failed return FIND_ERROR_NONE;

	direction *= ses->search_direction;
	p = doc_view->vs->y;
	height = doc_view->box.height;
	step = direction * height;

	if (ses->search_word) {
		if (!find_next_link_in_search(doc_view, direction))
			return FIND_ERROR_NONE;
		p += step;
	}

	if (!ses->search_word) {
		if (!ses->last_search_word) {
			return FIND_ERROR_NO_PREVIOUS_SEARCH;
		}
		ses->search_word = stracpy(ses->last_search_word);
		if (!ses->search_word) return FIND_ERROR_NONE;
	}

	get_search_data(doc_view->document);

	do {
		int in_range = is_in_range(doc_view->document, p, height,
					   ses->search_word, &min, &max);

		if (in_range == -1) return FIND_ERROR_MEMORY;
		if (in_range == -2) return FIND_ERROR_REGEX;
		if (in_range) {
			doc_view->vs->y = p;
			if (max >= min)
				doc_view->vs->x = int_min(int_max(doc_view->vs->x,
								  max - doc_view->box.width),
								  min);

			set_link(doc_view);
			find_next_link_in_search(doc_view, direction * 2);

			if (hit_top)
				return FIND_ERROR_HIT_TOP;

			if (hit_bottom)
				return FIND_ERROR_HIT_BOTTOM;

			return FIND_ERROR_NONE;
		}
		p += step;
		if (p > doc_view->document->height) {
			hit_bottom = 1;
			p = 0;
		}
		if (p < 0) {
			hit_top = 1;
			p = 0;
			while (p < doc_view->document->height) p += height;
			p -= height;
		}
		c += height;
	} while (c < doc_view->document->height + height);

	return FIND_ERROR_NOT_FOUND;
}

static void
print_find_error_not_found(struct session *ses, unsigned char *title,
			   unsigned char *message, unsigned char *search_string)
{
	switch (get_opt_int("document.browse.search.show_not_found")) {
		case 2:
			msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
				title, ALIGN_CENTER,
				msg_text(ses->tab->term, message,
					 search_string),
				NULL, 1,
				N_("OK"), NULL, B_ENTER | B_ESC);
			break;

		case 1:
			beep_terminal(ses->tab->term);

		default:
			break;
	}
}

static void
print_find_error(struct session *ses, enum find_error find_error)
{
	int hit_top = 0;
	unsigned char *message = NULL;

	switch (find_error) {
		case FIND_ERROR_HIT_TOP:
			hit_top = 1;
		case FIND_ERROR_HIT_BOTTOM:
			if (!get_opt_bool("document.browse.search"
					  ".show_hit_top_bottom"))
				break;

			message = hit_top
				 ? N_("Search hit top, continuing at bottom.")
				 : N_("Search hit bottom, continuing at top.");
			break;
		case FIND_ERROR_NO_PREVIOUS_SEARCH:
			message = N_("No previous search");
			break;
		case FIND_ERROR_NOT_FOUND:
			print_find_error_not_found(ses, N_("Search"),
						   N_("Search string"
						      " '%s' not found"),
						   ses->search_word);

			break;

		case FIND_ERROR_REGEX:
			print_find_error_not_found(ses, N_("Search"),
						   N_("Could not compile"
						      " regular expression"
						      " '%s'"),
						   ses->search_word);

			break;

		case FIND_ERROR_MEMORY:
			/* Why bother trying to create a msg_box?
			 * We probably don't have the memory... */
		case FIND_ERROR_NONE:
			break;
	}

	if (message)
		msg_box(ses->tab->term, NULL, 0,
			N_("Search"), ALIGN_CENTER,
			message,
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
}

enum frame_event_status
find_next(struct session *ses, struct document_view *doc_view, int direction)
{
	print_find_error(ses, find_next_do(ses, doc_view, direction));

	/* FIXME: Make this more fine-grained */
	return FRAME_EVENT_REFRESH;
}


/* Link typeahead */

enum typeahead_code {
	TYPEAHEAD_MATCHED,
	TYPEAHEAD_ERROR,
	TYPEAHEAD_CANCEL,
};

static void
typeahead_error(struct session *ses, unsigned char *typeahead)
{
	print_find_error_not_found(ses,
				   N_("Typeahead"),
				   N_("Could not find a link"
				      " with the text '%s'."),
				   typeahead);
}

static inline unsigned char *
get_link_typeahead_text(struct link *link)
{
	unsigned char *name = get_link_name(link);

	if (name) return name;
	if (link->where) return link->where;
	if (link->where_img) return link->where_img;

	return "";
}

/* Searches the @document for a link with the given @text. takes the
 * current_link in the view, the link to start searching from @i and the
 * direction to search (1 is forward, -1 is back). */
static inline int
search_link_text(struct document *document, int current_link, int i,
		 unsigned char *text, int direction, int *offset)
{
	int upper_link, lower_link;
	int case_sensitive = get_opt_bool("document.browse.search.case");
	int wraparound = get_opt_bool("document.browse.search.wraparound");
	int textlen = strlen(text);

	assert(textlen && direction && offset);

	/* The link interval in which we are currently searching */
	/* Set up the range of links that should be search in first attempt */
	if (direction > 0) {
		upper_link = document->nlinks;
		lower_link = i - 1;
	} else {
		upper_link = i + 1;
		lower_link = -1;
	}

#define match_link_text(t1, t2)					\
	(case_sensitive ? strstr(t1, t2) : strcasestr(t1, t2))

	for (; i > lower_link && i < upper_link; i += direction) {
		struct link *link = &document->links[i];
		unsigned char *match = get_link_typeahead_text(link);
		unsigned char *matchpos;

		if (link_is_form(link)
		    || textlen > strlen(match))
			continue;

		/* Did the text match? */
		matchpos = match_link_text(match, text);
		if (matchpos) {
			*offset = matchpos - match;
			return i;
		}

		if (!wraparound) continue;

		/* Check if we are at the end of the first range.
		 * Only wrap around one time. Initialize @i with
		 * {+= direction} in mind. */
		if (direction > 0) {
			 if (i == upper_link - 1) {
				upper_link = current_link + 1;
				lower_link = -1;
				i = lower_link;
				wraparound = 0;
			 }
		} else {
			if (i == lower_link + 1) {
				upper_link = document->nlinks;
				lower_link = current_link - 1;
				i = upper_link;
				wraparound = 0;
			}
		}
	}

#undef match_link_text

	return -1;
}

/* The typeahead input line takes up one of the viewed lines so we
 * might have to scroll if the link is under the input line. */
static inline void
fixup_typeahead_match(struct session *ses, struct document_view *doc_view)
{
	int current_link = doc_view->vs->current_link;
	struct link *link = &doc_view->document->links[current_link];

	doc_view->box.height -= 1;
	set_pos_x(doc_view, link);
	set_pos_y(doc_view, link);
	doc_view->box.height += 1;
}

static inline unsigned char
get_document_char(struct document *document, int x, int y)
{
	return (document->height > y && document->data[y].length > x)
		? document->data[y].chars[x].data : 0;
}

static inline void
draw_typeahead_match(struct terminal *term, struct document_view *doc_view,
		     int chars, int offset)
{
	struct color_pair *color = get_bfu_color(term, "searched");
	int xoffset = doc_view->box.x - doc_view->vs->x;
	int yoffset = doc_view->box.y - doc_view->vs->y;
	struct link *link = get_current_link(doc_view);
	unsigned char *text = get_link_typeahead_text(link);
	int end = offset + chars;
	int i, j;

	for (i = 0, j = 0; text[j] && i < end; i++, j++) {
		int x = link->points[i].x;
		int y = link->points[i].y;
		unsigned char data = get_document_char(doc_view->document, x, y);

		/* Text wrapping might remove space chars from the link
		 * position array so try to align the matched typeahead text
		 * with what is actually on the screen by shifting the link
		 * position variables if the canvas data do not match. */
		if (data != text[j]) {
			i--;
			end--;
			offset--;

		} else if (i >= offset) {
			/* TODO: We should take in account original colors and
			 * combine them with defined color. */
			draw_char_color(term, xoffset + x, yoffset + y, color);
		}
	}
}

static enum typeahead_code
do_typeahead(struct session *ses, struct document_view *doc_view,
	     unsigned char *text, int action, int *offset)
{
	int current = int_max(doc_view->vs->current_link, 0);
	int direction, match, i = current;
	struct document *document = doc_view->document;

	switch (action) {
		case ACT_EDIT_PREVIOUS_ITEM:
		case ACT_EDIT_UP:
			direction = -1;
			i--;
			if (i >= 0) break;
			if (!get_opt_bool("document.browse.search.wraparound"))
				return TYPEAHEAD_ERROR;

			i = doc_view->document->nlinks - 1;
			break;

		case ACT_EDIT_NEXT_ITEM:
		case ACT_EDIT_DOWN:
			direction = 1;
			i++;
			if (i < doc_view->document->nlinks) break;
			if (!get_opt_bool("document.browse.search.wraparound"))
				return TYPEAHEAD_ERROR;

			i = 0;
			break;

 		case ACT_EDIT_ENTER:
			goto_current_link(ses, doc_view, 0);
			return TYPEAHEAD_CANCEL;

		default:
			direction = 1;
	}

	match = search_link_text(document, current, i, text, direction, offset);
	if (match < 0) return TYPEAHEAD_ERROR;

	assert(match >= 0 && match < doc_view->document->nlinks);

	doc_view->vs->current_link = match;
	return TYPEAHEAD_MATCHED;
}


/* Typeahead */

static enum input_line_code
text_typeahead_handler(struct input_line *line, int action)
{
	struct session *ses = line->ses;
	unsigned char *buffer = line->buffer;
	struct document_view *doc_view = current_frame(ses);
	int direction = ((unsigned char *) line->data)[0] == '/' ? 1 : -1;
	int report_errors = action == -1;
	enum find_error error;

	assertm(doc_view, "document not formatted");
	if_assert_failed return INPUT_LINE_CANCEL;

	switch (action) {
		case ACT_EDIT_ENTER:
			if (!*buffer) {
				/* This ensures that search-typeahead-text
				 * followed immediately with enter
				 * clears the last search. */
				search_for_do(ses, buffer, direction, 0);
			}
			goto_current_link(ses, doc_view, 0);
			return INPUT_LINE_CANCEL;

		case ACT_EDIT_PREVIOUS_ITEM:
			find_next(ses, doc_view, -1);
			break;

		case ACT_EDIT_NEXT_ITEM:
			find_next(ses, doc_view, 1);
			break;

		case ACT_EDIT_SEARCH_TOGGLE_REGEX: {
			struct option *opt =
				get_opt_rec(config_options,
					    "document.browse.search.regex");

			opt->value.number = (opt->value.number + 1)
					    % (opt->max + 1);
			opt->flags |= OPT_TOUCHED;
		}
		/* Fall thru */

		default:
			error = search_for_do(ses, buffer, direction, 0);

			if (error == FIND_ERROR_REGEX)
				break;

			if (report_errors)
				print_find_error(ses, error);

			/* We need to check |*buffer| here because
			 * the input-line code will call this handler
			 * even after it handles a back-space press. */
			if (error != FIND_ERROR_NONE && *buffer)
				return INPUT_LINE_REWIND;
	}

	draw_formatted(ses, 0);
	return INPUT_LINE_PROCEED;
}

static enum input_line_code
link_typeahead_handler(struct input_line *line, int action)
{
	struct session *ses = line->ses;
	unsigned char *buffer = line->buffer;
	struct document_view *doc_view = current_frame(ses);
	int offset = 0;

	assertm(doc_view, "document not formatted");
	if_assert_failed return INPUT_LINE_CANCEL;

	/* If there is nothing to match with don't start searching */
	if (!*buffer) {
		/* If something already were typed we need to redraw
		 * in order to remove the coloring of the link text. */
		if (line->data) draw_formatted(ses, 0);
		return INPUT_LINE_PROCEED;
	}

	/* Hack time .. should we change mode? */
	if (!line->data) {
		enum main_action action = ACT_MAIN_NONE;

		switch (*buffer) {
			case '#':
				action = ACT_MAIN_SEARCH_TYPEAHEAD_LINK;
				break;

			case '?':
				action = ACT_MAIN_SEARCH_TYPEAHEAD_TEXT_BACK;
				break;

			case '/':
				action = ACT_MAIN_SEARCH_TYPEAHEAD_TEXT;
				break;

			default:
				break;
		}

		/* Should we reboot the input line .. (inefficient but easy) */
		if (action != ACT_MAIN_NONE) {
			search_typeahead(ses, doc_view, action);
			return INPUT_LINE_CANCEL;
		}

		line->data = "#";
	}

	switch (do_typeahead(ses, doc_view, buffer, action, &offset)) {
		case TYPEAHEAD_MATCHED:
			fixup_typeahead_match(ses, doc_view);
			draw_formatted(ses, 0);
			draw_typeahead_match(ses->tab->term, doc_view, strlen(buffer), offset);
			return INPUT_LINE_PROCEED;

		case TYPEAHEAD_ERROR:
			typeahead_error(ses, buffer);
			return INPUT_LINE_REWIND;

		case TYPEAHEAD_CANCEL:
		default:
			return INPUT_LINE_CANCEL;
	}
}

enum frame_event_status
search_typeahead(struct session *ses, struct document_view *doc_view,
		 int action)
{
	unsigned char *prompt = "#";
	unsigned char *data = NULL;
	input_line_handler handler = text_typeahead_handler;
	struct input_history *history = &search_history;

	switch (action) {
		case ACT_MAIN_SEARCH_TYPEAHEAD_TEXT:
			prompt = data = "/";
			break;

		case ACT_MAIN_SEARCH_TYPEAHEAD_TEXT_BACK:
			prompt = data = "?";
			break;

		case ACT_MAIN_SEARCH_TYPEAHEAD_LINK:
			data = "#";
			/* Falling forward .. good punk rock */
		case ACT_MAIN_SEARCH_TYPEAHEAD:
		default:
			if (doc_view->document->nlinks) {
				handler = link_typeahead_handler;
				history = NULL;
				break;
			}

			msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
				N_("Typeahead"), ALIGN_CENTER,
				msg_text(ses->tab->term,
					 N_("No links in current document")),
				NULL, 1,
				N_("OK"), NULL, B_ENTER | B_ESC);

			return FRAME_EVENT_OK;
	}

	input_field_line(ses, prompt, data, history, handler);
	return FRAME_EVENT_OK;
}


/* The dialog functions are clones of input_field() ones. Gross code
 * duplication. */
/* TODO: This is just hacked input_field(), containing a lot of generic crap
 * etc. The useless cruft should be blasted out. And it's quite ugly anyway,
 * a nice cleanup target ;-). --pasky */

enum search_option {
	SEARCH_OPT_REGEX,
	SEARCH_OPT_CASE,

	SEARCH_OPTIONS,
};

static struct option_resolver resolvers[] = {
	{ SEARCH_OPT_REGEX,	"document.browse.search.regex" },
	{ SEARCH_OPT_CASE,	"document.browse.search.case" },
};

struct search_dlg_hop {
	void *data;
	union option_value values[SEARCH_OPTIONS];
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

	commit_option_values(resolvers, config_options,
			     hop->values, SEARCH_OPTIONS);

	if (check_dialog(dlg_data)) return 1;

	add_to_input_history(dlg_data->dlg->widgets->info.field.history, text, 1);

	if (fn) fn(data, text);

	return cancel_dialog(dlg_data, widget_data);
}

/* XXX: @data is ignored. */
static void
search_dlg_do(struct terminal *term, struct memory_list *ml,
	      unsigned char *title, void *data,
	      struct input_history *history,
	      void (*fn)(void *, unsigned char *))
{
	struct dialog *dlg;
	unsigned char *field;
	struct search_dlg_hop *hop;
	unsigned char *text = _("Search for text", term);

	hop = mem_calloc(1, sizeof(struct search_dlg_hop));
	if (!hop) return;

	checkout_option_values(resolvers, config_options,
			       hop->values, SEARCH_OPTIONS);
	hop->data = data;

#define SEARCH_WIDGETS_COUNT 8
	dlg = calloc_dialog(SEARCH_WIDGETS_COUNT, MAX_STR_LEN);
	if (!dlg) {
		mem_free(hop);
		return;
	}

	dlg->title = _(title, term);
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.fit_datalen = 1;
	dlg->layout.float_groups = 1;
	dlg->udata = text;
	dlg->udata2 = hop;

	add_to_ml(&ml, hop, NULL);

	/* @field is automatically cleared by calloc() */
	field = get_dialog_offset(dlg, SEARCH_WIDGETS_COUNT);
	add_dlg_field(dlg, text, 0, 0, NULL, MAX_STR_LEN, field, history);

	add_dlg_radio(dlg, _("Normal search", term), 1, 0, hop->values[SEARCH_OPT_REGEX].number);
	add_dlg_radio(dlg, _("Regexp search", term), 1, 1, hop->values[SEARCH_OPT_REGEX].number);
	add_dlg_radio(dlg, _("Extended regexp search", term), 1, 2, hop->values[SEARCH_OPT_REGEX].number);
	add_dlg_radio(dlg, _("Case sensitive", term), 2, 1, hop->values[SEARCH_OPT_CASE].number);
	add_dlg_radio(dlg, _("Case insensitive", term), 2, 0, hop->values[SEARCH_OPT_CASE].number);

	add_dlg_button(dlg, B_ENTER, search_dlg_ok, _("OK", term), fn);
	add_dlg_button(dlg, B_ESC, search_dlg_cancel, _("Cancel", term), NULL);

	add_dlg_end(dlg, SEARCH_WIDGETS_COUNT);

	add_to_ml(&ml, dlg, NULL);
	do_dialog(term, dlg, ml);
}

enum frame_event_status
search_dlg(struct session *ses, struct document_view *doc_view, int direction)
{
	unsigned char *title;
	void *search_function;

	assert(direction);
	if_assert_failed return FRAME_EVENT_OK;

	if (direction > 0) {
		title = N_("Search");
		search_function = search_for;
	} else {
		title = N_("Search backward");
		search_function = search_for_back;
	}

	search_dlg_do(ses->tab->term, NULL,
		      title, ses,
		      &search_history,
		      search_function);

	return FRAME_EVENT_OK;
}

static enum evhook_status
search_history_write_hook(va_list ap, void *data)
{
	save_input_history(&search_history, SEARCH_HISTORY_FILENAME);
	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info search_history_hooks[] = {
	{ "periodic-saving", search_history_write_hook, NULL },

	NULL_EVENT_HOOK_INFO,
};

void
init_search_history(void)
{
	load_input_history(&search_history, SEARCH_HISTORY_FILENAME);
	register_event_hooks(search_history_hooks);
}

void
done_search_history(void)
{
	unregister_event_hooks(search_history_hooks);
	save_input_history(&search_history, SEARCH_HISTORY_FILENAME);
	free_list(search_history.entries);
}
