/* UNIX system-specific routines. */
/* $Id: unix.c,v 1.18 2004/07/28 15:43:51 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#ifdef UNIX

#ifdef HAVE_GPM_H
#include <gpm.h>
#endif

#include "elinks.h"

#include "lowlevel/select.h"
#include "osdep/unix/unix.h"
#include "osdep/osdep.h"
#include "terminal/event.h"
#include "terminal/mouse.h"
#include "util/memory.h"


#if defined(CONFIG_GPM) && defined(CONFIG_MOUSE)

struct gpm_mouse_spec {
	int h;
	int cons;
	void (*fn)(void *, unsigned char *, int);
	void *data;
};

static void
gpm_mouse_in(struct gpm_mouse_spec *gms)
{
	Gpm_Event gev;
	struct term_event ev;

	if (Gpm_GetEvent(&gev) <= 0) {
		set_handlers(gms->h, NULL, NULL, NULL, NULL);
		return;
	}

	ev.ev = EVENT_MOUSE;
	ev.info.mouse.x = gev.x - 1;
	ev.info.mouse.y = gev.y - 1;

	if (gev.buttons & GPM_B_LEFT)
		ev.info.mouse.button = B_LEFT;
	else if (gev.buttons & GPM_B_MIDDLE)
		ev.info.mouse.button = B_MIDDLE;
	else if (gev.buttons & GPM_B_RIGHT)
		ev.info.mouse.button = B_RIGHT;
	else
		return;

	if (gev.type & GPM_DOWN)
		ev.info.mouse.button |= B_DOWN;
	else if (gev.type & GPM_UP)
		ev.info.mouse.button |= B_UP;
	else if (gev.type & GPM_DRAG)
		ev.info.mouse.button |= B_DRAG;
	else
		return;

	gms->fn(gms->data, (char *) &ev, sizeof(struct term_event));
}

int
init_mouse(int cons, int suspend)
{
	Gpm_Connect conn;

	conn.eventMask = suspend ? 0 : ~GPM_MOVE;
	conn.defaultMask = suspend ? ~0 : GPM_MOVE;
	conn.minMod = suspend ? ~0 : 0;
	conn.maxMod = suspend ? ~0 : 0;

	return Gpm_Open(&conn, cons);
}

int
done_mouse(void)
{
	return Gpm_Close();
}

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	int h;
	struct gpm_mouse_spec *gms;

	h = init_mouse(cons, 0);
	if (h < 0) return NULL;

	gms = mem_alloc(sizeof(struct gpm_mouse_spec));
	if (!gms) return NULL;
	gms->h = h;
	gms->cons = cons;
	gms->fn = fn;
	gms->data = data;
	set_handlers(h, (void (*)(void *)) gpm_mouse_in, NULL, NULL, gms);

	return gms;
}

void
unhandle_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	set_handlers(gms->h, NULL, NULL, NULL, NULL);
	mem_free(gms);
	done_mouse();
}

void
suspend_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	gms->h = init_mouse(gms->cons, 1);
	if (gms->h < 0) return;

	set_handlers(gms->h, NULL, NULL, NULL, NULL);
}

void
resume_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	gms->h = init_mouse(gms->cons, 0);
	if (gms->h < 0) return;

	set_handlers(gms->h, (void (*)(void *)) gpm_mouse_in, NULL, NULL, gms);
}

#endif

#endif
