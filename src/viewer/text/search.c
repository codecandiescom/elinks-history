/* Searching in the HTML document */
/* $Id: search.c,v 1.3 2003/07/02 23:02:16 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/msgbox.h"
#include "document/html/renderer.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/search.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


/* FIXME: Add comments!! --Zas */


static
#ifdef __GNUCC__
inline			/* solaris CC bug */
#endif
int
srch_cmp(unsigned char c1, unsigned char c2)
{
	return strncasecmp(&c1, &c2, 1);
}

static int
get_range(struct f_data *f, int y, int yw, int l,
	  struct search **s1, struct search **s2)
{
	int i;

	assert(f && s1 && s2);

	*s1 = *s2 = NULL;
	for (i = y < 0 ? 0 : y; i < y + yw && i < f->y; i++) {
		if (f->slines1[i] && (!*s1 || f->slines1[i] < *s1))
			*s1 = f->slines1[i];
		if (f->slines2[i] && (!*s2 || f->slines2[i] > *s2))
			*s2 = f->slines2[i];
	}
	if (!*s1 || !*s2) return -1;

	*s1 -= l;

	if (*s1 < f->search)
		*s1 = f->search;
	if (*s2 + l > f->search + f->nsearch)
		*s2 = f->search + f->nsearch - l;
	if (*s1 > *s2)
		*s1 = *s2 = NULL;
	if (!*s1 || !*s2)
		return -1;

	return 0;
}

static int
is_in_range(struct f_data *f, int y, int yw, unsigned char *txt,
	    int *min, int *max)
{
	struct search *s1, *s2;
	int found = 0;
	int l;

	assert(f && txt && min && max);

	*min = MAXINT, *max = 0;
	l = strlen(txt);

	if (get_range(f, y, yw, l, &s1, &s2))
		return 0;

	for (; s1 <= s2; s1++) {
		register int i;

		if (srch_cmp(s1->c, txt[0])) {
srch_failed:
			continue;
		}

		for (i = 1; i < l; i++)
			if (srch_cmp(s1[i].c, txt[i]))
				goto srch_failed;

		if (s1[i].y < y || s1[i].y >= y + yw)
			continue;

		found = 1;

		for (i = 0; i < l; i++) {
			if (!s1[i].n) continue;

			if (s1[i].x < *min)
				*min = s1[i].x;
			if (s1[i].x + s1[i].n > *max)
				*max = s1[i].x + s1[i].n;
		}
	}
	return found;
}

static void
get_searched(struct f_data_c *scr, struct point **pt, int *pl)
{
	struct point *points = NULL;
	struct search *s1, *s2;
	int xp, yp;
	int xw, yw;
	int vx, vy;
	int l;
	int len = 0;
	unsigned char c;

	assert(scr && scr->vs && pt && pl);

	if (!scr->search_word || !*scr->search_word || !(*scr->search_word)[0])
		return;

	xp = scr->xp;
	yp = scr->yp;
	xw = scr->xw;
	yw = scr->yw;
	vx = scr->vs->view_posx;
	vy = scr->vs->view_pos;

	get_search_data(scr->f_data);
	l = strlen(*scr->search_word);
	if (get_range(scr->f_data, scr->vs->view_pos, scr->yw, l, &s1, &s2))
		goto ret;

	c = (*scr->search_word)[0];

	for (; s1 <= s2; s1++) {
		register int i;

		if (srch_cmp(s1->c, c)) {
srch_failed:
			continue;
		}

		for (i = 1; i < l; i++)
			if (srch_cmp(s1[i].c, (*scr->search_word)[i]))
				goto srch_failed;

		for (i = 0; i < l; i++) {
			register int j;
			int y = s1[i].y + yp - vy;

			if (y < yp || y >= yp + yw)
				continue;

			for (j = 0; j < s1[i].n; j++) {
				int x = s1[i].x + j + xp - vx;

				if (x < xp || x >= xp + xw)
					continue;

				if (!(len % ALLOC_GR)) {
					struct point *npt;

					npt = mem_realloc(points,
							  sizeof(struct point)
							  * (len + ALLOC_GR));

					if (!npt) continue;
					points = npt;
				}
				points[len].x = s1[i].x + j;
				points[len++].y = s1[i].y;
			}
		}
	}

ret:
	*pt = points;
	*pl = len;
}

/* Highlighting of searched strings. */
void
draw_searched(struct terminal *term, struct f_data_c *scr)
{
	struct point *pt = NULL;
	int color = 0;
	int len = 0;
	register int i;

	assert(term && scr);

	if (!scr->search_word || !*scr->search_word || !(*scr->search_word)[0])
		return;

	get_searched(scr, &pt, &len);
	if (len) color = get_bfu_color(term, "searched");

	for (i = 0; i < len; i++) {
		int x = pt[i].x + scr->xp - scr->vs->view_posx;
		int y = pt[i].y + scr->yp - scr->vs->view_pos;

#if 0 /* We should take in account orignal colors and combine them
	 with defined color. */
		unsigned co = get_char(term, x, y);
		co = ((co >> 3) & 0x0700) | ((co << 3) & 0x3800);
#endif

		set_color(term, x, y, color);
	}

	if (pt) mem_free(pt);
}



static void
search_for_do(struct session *ses, unsigned char *str, int direction)
{
	struct f_data_c *f;

	assert(ses && str);

	if (!*str) return;
	f = current_frame(ses);
	assert(f);

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
	search_for_do(ses, str, -1);
}

void
search_for(struct session *ses, unsigned char *str)
{
	assert(ses && str);
	search_for_do(ses, str, 1);
}

static inline int
point_intersect(struct point *p1, int l1, struct point *p2, int l2)
{
#define HASH_SIZE	4096
#define HASH(p) (((p.y << 6) + p.x) & (HASH_SIZE - 1))

	register int i, j;
	static char hash[HASH_SIZE];
	static int first_time = 1;

	assert(p1 && p2);

	if (first_time) memset(hash, 0, HASH_SIZE), first_time = 0;

	for (i = 0; i < l1; i++) hash[HASH(p1[i])] = 1;

	for (j = 0; j < l2; j++) if (hash[HASH(p2[j])]) {
		for (i = 0; i < l1; i++) if (p1[i].x == p2[j].x && p1[i].y == p2[j].y) {
			for (i = 0; i < l1; i++) hash[HASH(p1[i])] = 0;
			return 1;
		}
	}

	for (i = 0; i < l1; i++) hash[HASH(p1[i])] = 0;

	return 0;

#undef HASH
#undef HASH_SIZE
}

static int
find_next_link_in_search(struct f_data_c *f, int d)
{
	struct point *pt = NULL;
	struct link *link;
	int len;

	assert(f && f->vs);

	if (d == -2 || d == 2) {
		d /= 2;
		find_link(f, d, 0);
		if (f->vs->current_link == -1) return 1;
	} else nx:if (f->vs->current_link == -1
		      || !(next_in_view(f, f->vs->current_link + d, d, in_view, NULL))) {
		find_link(f, d, 0);
		return 1;
	}
	link = &f->f_data->links[f->vs->current_link];
	get_searched(f, &pt, &len);
	if (point_intersect(pt, len, link->pos, link->n)) {
		mem_free(pt);
		return 0;
	}
	if (pt) mem_free(pt);
	goto nx;
}

void
find_next(struct session *ses, struct f_data_c *f, int a)
{
	int p, min, max, c = 0;

	assert(ses && ses->tab && ses->tab->term && f && f->vs);

	p = f->vs->view_pos;

	if (!a && ses->search_word) {
		if (!(find_next_link_in_search(f, ses->search_direction))) return;
		p += ses->search_direction * f->yw;
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

	get_search_data(f->f_data);

	do {
		if (is_in_range(f->f_data, p, f->yw, ses->search_word, &min, &max)) {
			f->vs->view_pos = p;
			if (max >= min) {
				if (max > f->vs->view_posx + f->xw)
					f->vs->view_posx = max - f->xw;
				if (min < f->vs->view_posx)
					f->vs->view_posx = min;
			}
			set_link(f);
			find_next_link_in_search(f, ses->search_direction * 2);
#if 0
			draw_doc(ses->tab->term, f, 1);
			print_screen_status(ses);
			redraw_from_window(ses->tab);
#endif
			return;
		}
		p += ses->search_direction * f->yw;
		if (p > f->f_data->y) {
			/* TODO: A notice for user? --pasky */
			p = 0;
		}
		if (p < 0) {
			p = 0;
			while (p < f->f_data->y) p += f->yw;
			p -= f->yw;
		}
	} while ((c += f->yw) < f->f_data->y + f->yw);

#if 0
	draw_doc(ses->tab->term, f, 1);
	print_screen_status(ses);
	redraw_from_window(ses->tab);
#endif
	msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
		N_("Search"), AL_CENTER,
		msg_text(ses->tab->term, N_("Search string '%s' not found"),
			ses->search_word),
		ses, 1,
		N_("Cancel"), NULL, B_ENTER | B_ESC);
}

void
find_next_back(struct session *ses, struct f_data_c *f, int a)
{
	assert(ses && f);

	ses->search_direction = -ses->search_direction;
	find_next(ses, f, a);
	ses->search_direction = -ses->search_direction;
}
