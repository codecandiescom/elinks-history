/* Searching in the HTML document */
/* $Id: search.c,v 1.43 2003/10/06 23:35:30 jonas Exp $ */

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
	int case_insensitive = get_opt_int("document.browse.search.case")
				? 0 : REG_ICASE;
	int matches_may_overlap = get_opt_bool("document.browse.search.overlap");
	register int i;
	regex_t regex;
	regmatch_t regmatch;

	doclen = s2 - s1 + l;
	if (!doclen) return 0;
	doc = mem_alloc(sizeof(unsigned char) * (doclen + 1));
	if (!doc) return 0;
	
	for (i = 0; i < doclen; i++) {
		if (i > 0 && s1[i - 1].c == ' ' && s1[i - 1].y != s1[i].y) {
			doc[i - 1] = '\n';
		}
		doc[i] = s1[i].c;
	}
	doc[doclen] = 0;

	if (regcomp(&regex, text,
		    REG_NEWLINE | case_insensitive | reg_extended)) {
		mem_free(doc);
		return 0;
	}

	doctmp = doc;
	while (*doctmp && !regexec(&regex, doctmp, 1, &regmatch, 0)) {
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
	int case_sensitive = get_opt_int("document.browse.search.case");

	txt = case_sensitive ? stracpy(*scr->search_word)
			     : lowered_string(*scr->search_word, l);
	if (!txt) return;

	xp = scr->xp;
	yp = scr->yp;
	xx = xp + scr->xw;
	yy = yp + scr->yw;
	xpv= xp - scr->vs->view_posx;
	ypv= yp - scr->vs->view_pos;

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

#undef maybe_tolower

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
	int matches_may_overlap = get_opt_bool("document.browse.search.overlap");
	int regex_flags = REG_NEWLINE;
	register int i;
	regex_t regex;
	regmatch_t regmatch;

	doclen = s2 - s1 + l;
	if (!doclen) goto ret;
	doc = mem_alloc(sizeof(unsigned char) * (doclen + 1));
	if (!doc) goto ret;

	for (i = 0; i < doclen; i++) {
		if (i > 0 && s1[i - 1].c == ' ' && s1[i - 1].y != s1[i].y) {
			doc[i - 1] = '\n';
		}
		doc[i] = s1[i].c;
	}
	doc[doclen] = 0;

	if (get_opt_int("document.browse.search.regex") == 2)
		regex_flags |= REG_EXTENDED; 

	if (get_opt_bool("document.browse.search.case"))
		regex_flags |= REG_ICASE;

	/* TODO: show error message */
	if (regcomp(&regex, *scr->search_word, regex_flags)) {
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

	f = current_frame(ses);

	assert(f);
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
				N_("OK"), NULL, B_ENTER | B_ESC);
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
		N_("OK"), NULL, B_ENTER | B_ESC);
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

/* The dialog functions are clones of input_field() ones. Gross code
 * duplication. */
/* TODO: This is just hacked input_field(), containing a lot of generic crap
 * etc. The useless cruft should be blasted out. And it's quite ugly anyway,
 * a nice cleanup target ;-). --pasky */

static unsigned char *regex_labels[] = {
	N_("Normal search"),
	N_("Regexp search"),
	N_("Extended regexp search"),
	NULL
};

static unsigned char *case_labels[] = {
	N_("Case sensitive"),
	N_("Case insensitive"),
	NULL
};

struct search_dlg_hop {
	void *data;
	int whether_regex, cases;
};

static int
search_dlg_cancel(struct dialog_data *dlg, struct widget_data *di)
{
	void (*fn)(void *) = di->item->udata;
	struct search_dlg_hop *hop = dlg->dlg->udata2;
	void *data = hop->data;

	if (fn) fn(data);
	cancel_dialog(dlg, di);

	return 0;
}

static int
search_dlg_ok(struct dialog_data *dlg, struct widget_data *di)
{
	void (*fn)(void *, unsigned char *) = di->item->udata;
	struct search_dlg_hop *hop = dlg->dlg->udata2;
	void *data = hop->data;
	unsigned char *text = dlg->items->cdata;

	update_dialog_data(dlg, di);

	/* TODO: Some generic update_opt() or so. --pasky */

	{
		struct option *o = get_opt_rec(config_options,
					       "document.browse.search.regex");

		if (*((int *) o->ptr) != hop->whether_regex) {
			*((int *) o->ptr) = hop->whether_regex;
			o->flags |= OPT_TOUCHED;
		}
	}

	{
		struct option *o = get_opt_rec(config_options,
					       "document.browse.search.case");

		if (*((int *) o->ptr) != hop->cases) {
			*((int *) o->ptr) = hop->cases;
			o->flags |= OPT_TOUCHED;
		}
	}

	if (check_dialog(dlg)) return 1;

	add_to_input_history(dlg->dlg->items->history, text, 1);

	if (fn) fn(data, text);
	ok_dialog(dlg, di);
	return 0;
}

void
search_dlg_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct color_pair *text_color = get_bfu_color(term, "dialog.text");

	text_width(term, dlg->dlg->udata, &min, &max);
	/* I'm leet! --pasky */
	max_group_width(term, 1, regex_labels, dlg->items + 1, 3, &max);
	min_group_width(term, 1, regex_labels, dlg->items + 1, 3, &min);
	max_group_width(term, 1, case_labels, dlg->items + 4, 2, &max);
	min_group_width(term, 1, case_labels, dlg->items + 4, 2, &min);
	buttons_width(term, dlg->items + 6, 2, &min, &max);

	if (max < dlg->dlg->items->dlen) max = dlg->dlg->items->dlen;

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;

	rw = 0; /* !!! FIXME: input field */
	dlg_format_text(NULL, term, dlg->dlg->udata, 0, &y, w, &rw,
			text_color, AL_LEFT);
	dlg_format_field(NULL, term, dlg->items, 0, &y, w, &rw,
			 AL_LEFT);

	y++;
	dlg_format_group(NULL, term, 1, regex_labels, dlg->items + 1, 3, 0,
			 &y, w, &rw);

	y++;
	dlg_format_group(NULL, term, 1, case_labels, dlg->items + 4, 2, 0,
			 &y, w, &rw);

	y++;
	dlg_format_buttons(NULL, term, dlg->items + 6, 2, 0, &y, w, &rw,
			   AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term, dlg->dlg->udata, dlg->x + DIALOG_LB,
			&y, w, NULL, text_color, AL_LEFT);
	dlg_format_field(term, term, dlg->items, dlg->x + DIALOG_LB,
			 &y, w, NULL, AL_LEFT);

	y++;
	dlg_format_group(term, term, 1, regex_labels, dlg->items + 1, 3,
			 dlg->x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_group(term, term, 1, case_labels, dlg->items + 4, 2,
			 dlg->x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_buttons(term, term, dlg->items + 6, 2, dlg->x + DIALOG_LB,
			   &y, w, NULL, AL_CENTER);
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

#define SIZEOF_DIALOG (sizeof(struct dialog) + 4 * sizeof(struct widget))

	dlg = mem_calloc(1, SIZEOF_DIALOG + l);
	if (!dlg) {
		mem_free(hop);
		return;
	}

	field = (unsigned char *) dlg + SIZEOF_DIALOG;
	*field = 0;

#undef SIZEOF_DIALOG

	if (def) {
		int defsize = strlen(def) + 1;

		memcpy(field, def, (defsize > l) ? l - 1 : defsize);
	}

	dlg->title = title;
	dlg->fn = search_dlg_fn;
	dlg->udata = text;
	dlg->udata2 = hop;

	add_to_ml(&ml, hop, NULL);

	dlg->items[0].type = D_FIELD;
	dlg->items[0].gid = min;
	dlg->items[0].gnum = max;
	dlg->items[0].fn = check;
	dlg->items[0].history = history;
	dlg->items[0].dlen = l;
	dlg->items[0].data = field;

	dlg->items[1].type = D_CHECKBOX;	
	dlg->items[1].gid = 1;
	dlg->items[1].gnum = 0;
	dlg->items[1].dlen = sizeof(int);
	dlg->items[1].data = (unsigned char *) &hop->whether_regex;

	dlg->items[2].type = D_CHECKBOX;	
	dlg->items[2].gid = 1;
	dlg->items[2].gnum = 1;
	dlg->items[2].dlen = sizeof(int);
	dlg->items[2].data = (unsigned char *) &hop->whether_regex;

	dlg->items[3].type = D_CHECKBOX;	
	dlg->items[3].gid = 1;
	dlg->items[3].gnum = 2;
	dlg->items[3].dlen = sizeof(int);
	dlg->items[3].data = (unsigned char *) &hop->whether_regex;

	dlg->items[4].type = D_CHECKBOX;	
	dlg->items[4].gid = 2;
	dlg->items[4].gnum = 1;
	dlg->items[4].dlen = sizeof(int);
	dlg->items[4].data = (unsigned char *) &hop->cases;

	dlg->items[5].type = D_CHECKBOX;	
	dlg->items[5].gid = 2;
	dlg->items[5].gnum = 0;
	dlg->items[5].dlen = sizeof(int);
	dlg->items[5].data = (unsigned char *) &hop->cases;

	dlg->items[6].type = D_BUTTON;
	dlg->items[6].gid = B_ENTER;
	dlg->items[6].fn = search_dlg_ok;
	dlg->items[6].dlen = 0;
	dlg->items[6].text = okbutton;
	dlg->items[6].udata = fn;

	dlg->items[7].type = D_BUTTON;
	dlg->items[7].gid = B_ESC;
	dlg->items[7].fn = search_dlg_cancel;
	dlg->items[7].dlen = 0;
	dlg->items[7].text = cancelbutton;
	dlg->items[7].udata = cancelfn;

	dlg->items[8].type = D_END;

	add_to_ml(&ml, dlg, NULL);
	do_dialog(term, dlg, ml);
}


void
search_back_dlg(struct session *ses, struct document_view *f, int a)
{
	search_dlg_do(ses->tab->term, NULL, 1,
		    N_("Search backward"), N_("Search for text"),
		    N_("OK"), N_("Cancel"), ses, &search_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) search_for_back,
		    NULL);
}

void
search_dlg(struct session *ses, struct document_view *f, int a)
{
	search_dlg_do(ses->tab->term, NULL, 1,
		    N_("Search"), N_("Search for text"),
		    N_("OK"), N_("Cancel"), ses, &search_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) search_for,
		    NULL);
}

void
load_search_history(void)
{
	load_input_history(&search_history, "searchhist");
}

/* TODO: Keep dirty state of the search history. */
void
save_search_history(void)
{
	save_input_history(&search_history, "searchhist");
}
