/* Marks registry */
/* $Id: marks.c,v 1.1 2003/11/25 22:55:03 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/view.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


/* TODO list:
 *
 * * Make it possible to go at marks which are set in a different document than
 * the active one. This will basically need some clever hacking in the
 * KP_MARK_GOTO handler, which first needs to load the URL and *then* jump at
 * the exact location and just overally restore the rest of view_state. Perhaps
 * this could also be somehow nicely unified with session.goto_position? --pasky
 *
 * * The lower-case chars should have per-document scope, while the upper-case
 * chars would have per-ELinks (over all instances as well) scope. The l-c marks
 * should be stored either in {struct document} or {struct location}, that needs
 * further investigation. Disappearing when document gets out of all caches
 * and/or histories is not an issue from my POV. However, it must be ensured
 * that all instances of the document (and only the document) share the same
 * marks. If we are dealing with frames, I mean content of one frame by
 * 'document' - each frame in the frameset gets own set of marks. --pasky
 *
 * * Number marks for last ten documents in session history. XXX: To be
 * meaningful, it needs to support last n both history and unhistory items.
 * --pasky
 *
 * * "''" support (jump to the last visited mark). (What if it was per-document
 * mark and we are already in another document now?) --pasky
 *
 * * When pressing "'", list of already set marks should appear. The next
 * pressed char, if letter, would still directly get stuff from the list.
 * --pasky */


/* The @marks array is indexed by ASCII code of the mark. */
/* TODO: Shrink the array not to waste memory by plenty of fields we are never
 * going to use, but do not overcomplicate it. It must be reasonably easy to
 * add slots for other marks in the future. --pasky */
static struct view_state *marks[128];

struct view_state *
get_mark(unsigned char mark)
{
	assert(mark < 128);

	if (!((mark >= 'A' && mark <= 'Z') || (mark >= 'a' && mark <= 'z')))
		return NULL;

	return marks[mark];
}

void
set_mark(unsigned char mark, struct view_state *mark_vs)
{
	struct view_state *vs;

	assert(mark < 128);

	if (!((mark >= 'A' && mark <= 'Z') || (mark >= 'a' && mark <= 'z')))
		return;

	if (marks[mark]) {
		destroy_vs(marks[mark]);
		mem_free(marks[mark]);
		marks[mark] = NULL;
	}

	if (!mark_vs) return;

	vs = mem_alloc(sizeof(struct view_state) + mark_vs->url_len);
	if (!vs) return;
	copy_vs(vs, mark_vs);

	marks[mark] = vs;
}

void
free_marks(void)
{
	int i;

	for (i = 0; i < 128; i++) {
		if (!marks[i]) continue;
		destroy_vs(marks[i]);
		mem_free(marks[i]);
		marks[i] = NULL;
	}
}
