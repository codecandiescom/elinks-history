/* Searching in the HTML document */
/* $Id: search.c,v 1.27 2003/10/04 22:21:35 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h> /* tolower() */
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/inpfield.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
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
add_srch_chr(struct document *f, unsigned char c, int x, int y, int nn)
{
	int n;

	assert(f);
	if_assert_failed return;

	n = f->nsearch;

	if (c == ' ' && (!n || f->search[n - 1].c == ' ')) return;
	f->search[n].c = c;
	f->search[n].x = x;
	f->search[n].y = y;
	f->search[n].n = nn;
	f->nsearch++;
}

#if 0
/* Debugging code, please keep it. */
void
sdbg(struct document *f)
{
	struct node *n;

	foreachback (n, f->nodes) {
		int xm = n->x + n->xw, ym = n->y + n->yw;
		printf("%d %d - %d %d\n", n->x, n->y, xm, ym);
		fflush(stdout);
	}
	debug("!");
}
#endif


static void
sort_srch(struct document *f)
{
	int i;
	int *min, *max;

	assert(f);
	if_assert_failed return;

	f->slines1 = mem_calloc(f->y, sizeof(struct search *));
	if (!f->slines1) return;

	f->slines2 = mem_calloc(f->y, sizeof(struct search *));
	if (!f->slines2) {
		mem_free(f->slines1);
		return;
	}

	min = mem_calloc(f->y, sizeof(int));
	if (!min) {
		mem_free(f->slines1);
		mem_free(f->slines2);
		return;
	}

	max = mem_calloc(f->y, sizeof(int));
	if (!max) {
		mem_free(f->slines1);
		mem_free(f->slines2);
		mem_free(min);
		return;
	}

	for (i = 0; i < f->y; i++) {
		min[i] = MAXINT;
		max[i] = 0;
	}

	for (i = 0; i < f->nsearch; i++) {
		struct search *s = &f->search[i];
		int sxn = s->x + s->n;

		if (s->x < min[s->y]) {
			min[s->y] = s->x;
		   	f->slines1[s->y] = s;
		}
		if (sxn > max[s->y]) {
			max[s->y] = sxn;
			f->slines2[s->y] = s;
		}
	}

	mem_free(min);
	mem_free(max);
}

static int
get_srch(struct document *f)
{
	struct node *n;
	int cnt = 0;
	int cc;

	assert(f);
	if_assert_failed return 0;

	cc = !f->search;

	foreachback (n, f->nodes) {
		register int x, y;
		int xm = n->x + n->xw;
		int ym = n->y + n->yw;

#if 0
		printf("%d %d - %d %d\n", n->x, n->y, xm, ym);
		fflush(stdout);
#endif
#define ADD(cr, nn) do { if (!cc) add_srch_chr(f, (cr), x, y, (nn)); else cnt++; } while (0)

		for (y = n->y; y < ym && y < f->y; y++) {
			int ns = 1;

			for (x = n->x; x < xm && x < f->data[y].l; x++) {
				unsigned char c = f->data[y].d[x].data;

				if (c < ' ') c = ' ';
				if (c == ' ' && ns) continue;

				if (ns) {
					ADD(c, 1);
					ns = 0;
					continue;
				}

				if (c != ' ') {
					ADD(c, 1);
				} else {
					int xx;
					int found = 0;

					for (xx = x + 1;
					     xx < xm && xx < f->data[y].l;
					     xx++) {
						if (f->data[y].d[xx].data >= ' ') {
							found = 1;
							break;
						}
					}

					if (found) {
						ADD(' ', xx - x);
						x = xx - 1;
					} else {
						ADD(' ', 0);
						break;
					}
				}

			}

			ADD(' ', 0);
		}
#undef ADD

	}


	return cnt;
}

static void
get_search_data(struct document *f)
{
	int n;

	assert(f);
	if_assert_failed return;

	if (f->search) return;

	n = get_srch(f);
	if (!n) return;

	f->nsearch = 0;

	f->search = mem_alloc(n * sizeof(struct search));
	if (!f->search) return;

	get_srch(f);
	while (f->nsearch && f->search[--f->nsearch].c == ' ');
	sort_srch(f);
}

static int
get_range(struct document *f, int y, int yw, int l,
	  struct search **s1, struct search **s2)
{
	register int i;

	assert(f && s1 && s2);
	if_assert_failed return -1;

	*s1 = *s2 = NULL;
	int_lower_bound(&y, 0);

	for (i = y; i < y + yw && i < f->y; i++) {
		if (f->slines1[i] && (!*s1 || f->slines1[i] < *s1))
			*s1 = f->slines1[i];
		if (f->slines2[i] && (!*s2 || f->slines2[i] > *s2))
			*s2 = f->slines2[i];
	}
	if (!*s1 || !*s2) return -1;

	*s1 -= l;

	if (*s1 < f->search)
		*s1 = f->search;
	if (*s2 > f->search + f->nsearch - l + 1)
		*s2 = f->search + f->nsearch - l + 1;
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
is_in_range_regex(struct document *f, int y, int yy, unsigned char *text, int l,
		  int *min, int *max, struct search *s1, struct search *s2)
{
	unsigned char *doc;
	unsigned char *doctmp;
	int doclen;
	int found = 0;
	int reg_extended = get_opt_int("document.browse.search.regex") == 2
			   ? REG_EXTENDED : 0;
	int matches_may_overlap = get_opt_bool("document.browse.search.overlap");
	register int i;
	regex_t regex;
	regmatch_t regmatch;

	doclen = s2 - s1 + l;
	doc = mem_alloc(sizeof(unsigned char) * (doclen + 1));
	if (!doc) return 0;
	
	for (i = 0; i < doclen; i++)
		doc[i] = s1[i].c;
	doc[doclen] = 0;

	if (regcomp(&regex, text, REG_ICASE | reg_extended)) {
		mem_free(doc);
		return 0;
	}

	doctmp = doc;
	while (!regexec(&regex, doctmp, 1, &regmatch, 0)) {
		l = regmatch.rm_eo - regmatch.rm_so;
		s1 += regmatch.rm_so;
		doctmp += regmatch.rm_so;

		if (s1[l].y < y || s1[l].y >= yy)
			break;

		found = 1;

		for (i = 0; i < l; i++) {
			if (!s1[i].n) continue;

			int_upper_bound(min, s1[i].x);
			int_lower_bound(max, s1[i].x + s1[i].n);
		}

		if (matches_may_overlap) {
			doctmp++;
			s1++;
		} else {
			doctmp += int_max(l, 1);
			s1 += int_max(l, 1);
		}
	}

	regfree(&regex);
	mem_free(doc);

	return found;
}
#endif /* HAVE_REGEX_H */

static int
is_in_range_plain(struct document *f, int y, int yy, unsigned char *text, int l,
		  int *min, int *max, struct search *s1, struct search *s2)
{
	unsigned char *txt;
	int found = 0;

	txt = lowered_string(text, l);
	if (!txt) return 0;

	for (; s1 <= s2; s1++) {
		register int i;

		if (tolower(s1->c) != txt[0]) {
srch_failed:
			continue;
		}

		for (i = 1; i < l; i++)
			if (tolower(s1[i].c) != txt[i])
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

	mem_free(txt);

	return found;
}

static int
is_in_range(struct document *f, int y, int yw, unsigned char *text,
	    int *min, int *max)
{
	struct search *s1, *s2;
	int l;

	assert(f && text && min && max);
	if_assert_failed return 0;

	*min = MAXINT, *max = 0;
	l = strlen(text);

	if (get_range(f, y, yw, l, &s1, &s2))
		return 0;

#ifdef HAVE_REGEX_H
	if (get_opt_int("document.browse.search.regex"))
		return is_in_range_regex(f, y, y + yw, text, l, min, max, s1, s2);
#endif
	return is_in_range_plain(f, y, y + yw, text, l, min, max, s1, s2);
}

#define realloc_points(pts, size) \
	mem_align_alloc(pts, size, (size) + 1, sizeof(struct point), 0xFF)

static void
get_searched_plain(struct document_view *scr, struct point **pt, int *pl,
		   int l, struct search *s1, struct search *s2)
{
	unsigned char *txt;
	struct point *points = NULL;
	int xp, yp;
	int xx, yy;
	int xpv, ypv;
	int len = 0;

	txt = lowered_string(*scr->search_word, l);
	if (!txt) return;

	xp = scr->xp;
	yp = scr->yp;
	xx = xp + scr->xw;
	yy = yp + scr->yw;
	xpv= xp - scr->vs->view_posx;
	ypv= yp - scr->vs->view_pos;

	for (; s1 <= s2; s1++) {
		register int i;

		if (tolower(s1[0].c) != txt[0]) {
srch_failed:
			continue;
		}

		for (i = 1; i < l; i++)
			if (tolower(s1[i].c) != txt[i])
				goto srch_failed;

		for (i = 0; i < l; i++) {
			register int j;
			int y = s1[i].y + ypv;

			if (y < yp || y >= yy)
				continue;

			for (j = 0; j < s1[i].n; j++) {
				int sx = s1[i].x + j;
				int x = sx + xpv;

				if (x < xp || x >= xx)
					continue;

				if (!realloc_points(&points, len))
					continue;

				points[len].x = sx;
				points[len++].y = s1[i].y;
			}
		}
	}

	mem_free(txt);
	*pt = points;
	*pl = len;
}

#ifdef HAVE_REGEX_H
static void
get_searched_regex(struct document_view *scr, struct point **pt, int *pl,
		   int l, struct search *s1, struct search *s2)
{
	unsigned char *doc;
	unsigned char *doctmp;
	int doclen;
	struct point *points = NULL;
	int xp, yp;
	int xx, yy;
	int xpv, ypv;
	int len = 0;
	int reg_extended = get_opt_int("document.browse.search.regex") == 2
			   ? REG_EXTENDED : 0;
	int matches_may_overlap = get_opt_bool("document.browse.search.overlap");
	register int i;
	regex_t regex;
	regmatch_t regmatch;

	doclen = s2 - s1 + l;
	doc = mem_alloc(sizeof(unsigned char) * (doclen + 1));
	if (!doc) goto ret;
	
	for (i = 0; i < doclen; i++)
		doc[i] = s1[i].c;

	doc[doclen] = 0;

	/* TODO: show error message */
	if (regcomp(&regex, *scr->search_word, REG_ICASE | reg_extended)) {
		mem_free(doc);
		goto ret;
	}

	xp = scr->xp;
	yp = scr->yp;
	xx = xp + scr->xw;
	yy = yp + scr->yw;
	xpv= xp - scr->vs->view_posx;
	ypv= yp - scr->vs->view_pos;

	doctmp = doc;
	while (*doctmp && !regexec(&regex, doctmp, 1, &regmatch, 0)) {
		l = regmatch.rm_eo - regmatch.rm_so;
		s1 += regmatch.rm_so;
		doctmp += regmatch.rm_so;

		for (i = 0; i < l; i++) {
			register int j;
			int y = s1[i].y + ypv;

			if (y < yp || y >= yy)
				continue;

			for (j = 0; j < s1[i].n; j++) {
				int sx = s1[i].x + j;
				int x = sx + xpv;

				if (x < xp || x >= xx)
					continue;

				if (!realloc_points(&points, len))
					continue;

				points[len].x = sx;
				points[len++].y = s1[i].y;
			}
		}

		if (matches_may_overlap) {
			doctmp++;
			s1++;
		} else {
			doctmp += int_max(l, 1);
			s1 += int_max(l, 1);
		}
	}

	regfree(&regex);
	mem_free(doc);
ret:
	*pt = points;
	*pl = len;
}
#endif /* HAVE_REGEX_H */

static void
get_searched(struct document_view *scr, struct point **pt, int *pl)
{
	struct search *s1, *s2;
	int l;

	assert(scr && scr->vs && pt && pl);
	if_assert_failed return;

	if (!scr->search_word || !*scr->search_word || !(*scr->search_word)[0])
		return;

	get_search_data(scr->document);
	l = strlen(*scr->search_word);
	if (get_range(scr->document, scr->vs->view_pos, scr->yw, l, &s1, &s2)) {
		*pt = NULL;
		*pl = 0;

		return;
	}

#ifdef HAVE_REGEX_H
	if (get_opt_int("document.browse.search.regex"))
		get_searched_regex(scr, pt, pl, l, s1, s2);
	else
#endif
		get_searched_plain(scr, pt, pl, l, s1, s2);
}

/* Highlighting of searched strings. */
void
draw_searched(struct terminal *term, struct document_view *scr)
{
	struct point *pt = NULL;
	int len = 0;

	assert(term && scr);
	if_assert_failed return;

	if (!scr->search_word || !*scr->search_word || !(*scr->search_word)[0])
		return;

	get_searched(scr, &pt, &len);
	if (len) {
		register int i;
		struct color_pair *color = get_bfu_color(term, "searched");
		int xoffset = scr->xp - scr->vs->view_posx;
		int yoffset = scr->yp - scr->vs->view_pos;

		for (i = 0; i < len; i++) {
			int x = pt[i].x + xoffset;
			int y = pt[i].y + yoffset;

#if 0 /* We should take in account original colors and combine them
	 with defined color. */
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
	struct document_view *f;

	assert(ses && str);
	if_assert_failed return;

	if (!*str) return;
	f = current_frame(ses);

	assert(f);
	if_assert_failed return;

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
	find_next(ses, f, 1);
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
find_next_link_in_search(struct document_view *f, int d)
{
	struct point *pt = NULL;
	struct link *link;
	int len;

	assert(f && f->vs);
	if_assert_failed return 0;

	if (d == -2 || d == 2) {
		d /= 2;
		find_link(f, d, 0);
		if (f->vs->current_link == -1) return 1;
	} else nx:if (f->vs->current_link == -1
		      || !(next_in_view(f, f->vs->current_link + d, d, in_view, NULL))) {
		find_link(f, d, 0);
		return 1;
	}
	link = &f->document->links[f->vs->current_link];
	get_searched(f, &pt, &len);
	if (point_intersect(pt, len, link->pos, link->n)) {
		mem_free(pt);
		return 0;
	}
	if (pt) mem_free(pt);
	goto nx;
}

void
find_next(struct session *ses, struct document_view *f, int a)
{
	int p, min, max, c = 0;
	int step;

	assert(ses && ses->tab && ses->tab->term && f && f->vs);
	if_assert_failed return;

	p = f->vs->view_pos;
	step = ses->search_direction * f->yw;

	if (!a && ses->search_word) {
		if (!(find_next_link_in_search(f, ses->search_direction))) return;
		p += step;
	}

	if (!ses->search_word) {
		if (!ses->last_search_word) {
			msg_box(ses->tab->term, NULL, 0,
				N_("Search"), AL_CENTER,
				N_("No previous search"),
				NULL, 1,
				N_("Cancel"), NULL, B_ENTER | B_ESC);
			return;
		}
		ses->search_word = stracpy(ses->last_search_word);
		if (!ses->search_word) return;
	}

	get_search_data(f->document);

	do {
		if (is_in_range(f->document, p, f->yw, ses->search_word, &min, &max)) {
			f->vs->view_pos = p;
			if (max >= min)
				f->vs->view_posx = int_min(int_max(f->vs->view_posx,
								   max - f->xw),
							   min);

			set_link(f);
			find_next_link_in_search(f, ses->search_direction * 2);
			return;
		}
		p += step;
		if (p > f->document->y) {
			/* TODO: A notice for user? --pasky */
			p = 0;
		}
		if (p < 0) {
			p = 0;
			while (p < f->document->y) p += f->yw;
			p -= f->yw;
		}
		c += f->yw;
	} while (c < f->document->y + f->yw);

	msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
		N_("Search"), AL_CENTER,
		msg_text(ses->tab->term, N_("Search string '%s' not found"),
			ses->search_word),
		ses, 1,
		N_("Cancel"), NULL, B_ENTER | B_ESC);
}

void
find_next_back(struct session *ses, struct document_view *f, int a)
{
	assert(ses && f);
	if_assert_failed return;

	ses->search_direction = -ses->search_direction;
	find_next(ses, f, a);
	ses->search_direction = -ses->search_direction;
}


struct input_history search_history = { 0, {D_LIST_HEAD(search_history.items)} };

void
search_back_dlg(struct session *ses, struct document_view *f, int a)
{
	input_field(ses->tab->term, NULL, 1,
		    N_("Search backward"), N_("Search for text"),
		    N_("OK"), N_("Cancel"), ses, &search_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) search_for_back,
		    NULL);
}

void
search_dlg(struct session *ses, struct document_view *f, int a)
{
	input_field(ses->tab->term, NULL, 1,
		    N_("Search"), N_("Search for text"),
		    N_("OK"), N_("Cancel"), ses, &search_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) search_for,
		    NULL);
}
