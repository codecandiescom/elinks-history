/* Event handling functions */
/* $Id: event.c,v 1.2 2003/09/19 14:43:43 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "sched/event.h"
#include "util/error.h"
#include "util/hash.h"
#include "util/memory.h"
#include "util/string.h"


/* First, we should set some terminology:
 *
 * o event -> Message being [triggerred] by some ELinks part and [catched]
 *   by various random other ones.
 *
 * o event hook -> Device being deployed by various ELinks parts,
 *   associated with certain [event It [catches] that event by having a
 *   handler executed when the [event] is triggerred.
 *
 * o event chain -> Line of [event hook]s associated with a given [event
 *   The [hook]s are ordered by [priority Each hook returns whenever should
 *   the chain continue or that no other events in the chain should be
 *   triggered (TODO).
 */

struct event_handler {
	/* The function to be called with the event data. */
	int (*callback)(va_list ap);

	/* The @priority of this handler. */
	int priority;
};

struct event {
	/* The event name has to be unique. */
	unsigned char *name;

	/* There are @count event @handlers all ordered by priority. */
	struct event_handler *handlers;
	unsigned int count;

	/* The unique event id and position in events. */
	int id;
};

static struct event *events = NULL;
static unsigned int eventssize = 0;
static struct hash *event_hash = NULL;

/* TODO: This should be tuned to the number of events.  When we will have a lot
 * of them, this should be some big enough number to reduce unneccessary
 * slavery of CPU on the startup. Then after all main modules will be
 * initialized and their events will be registered, we could call something
 * like adjust_events_list() which will tune it to the exactly needed number.
 * This should be also called after each new plugin loaded. */
#define EVENT_GRANULARITY 0x07

#define realloc_events() \
	mem_align_alloc(&events, eventssize, eventssize + 1, \
			sizeof(struct event), EVENT_GRANULARITY)

static inline int
invalid_event_id(register int id)
{
	return (id == EVENT_NONE || id < 0 || id >= eventssize);
}

int
register_event(unsigned char *name)
{
	int id = get_event_id(name);
	struct event *event;
	int namelen;

	if (id != EVENT_NONE) return id;
	if (!realloc_events()) return EVENT_NONE;

	event = &events[eventssize];

	namelen = strlen(name);
	event->name = memacpy(name, namelen);
	if (!event->name) return EVENT_NONE;

	if (!add_hash_item(event_hash, event->name, namelen, event)) {
		mem_free(event->name);
		event->name = NULL;
		return EVENT_NONE;
	}

	event->handlers = NULL;
	event->count = 0;
	event->id = eventssize++;

	return event->id;
}

int
get_event_id(unsigned char *name)
{
	struct hash_item *item;
	int namelen;

	assertm(name && name[0], "Empty or missing event name");
	if_assert_failed return EVENT_NONE;

	if (!event_hash) return EVENT_NONE;

	namelen = strlen(name);
	item = get_hash_item(event_hash, name, namelen);
	if (item) {
		struct event *event = item->value;

		assertm(event, "Hash item with no value");
		if_assert_failed return EVENT_NONE;

		return event->id;
	}

	return EVENT_NONE;
}

unsigned char *
get_event_name(int id)
{
	if (invalid_event_id(id)) return NULL;

	return events[id].name;
}

void
trigger_event(int id, ...)
{
	register int i;
	struct event_handler *ev_handler;
	va_list ap;

	if (invalid_event_id(id)) return;

	ev_handler = events[id].handlers;
	for (i = 0; i < events[id].count; i++, ev_handler++) {
		int ret;

		va_start(ap, id);
		ret = ev_handler->callback(ap);
		va_end(ap);

		if (ret) return;
	}
}

static inline void
move_event_handler(struct event *event, int to, int from)
{
	int d = int_max(to, from);

	memmove(&event->handlers[to], &event->handlers[from],
		(event->count - d) * sizeof(struct event_handler));
}

int
register_event_hook(int id, int (*callback)(va_list ap), int priority)
{
	struct event *event;
	register int i;

	assert(callback);
	if_assert_failed return EVENT_NONE;

	if (invalid_event_id(id)) return EVENT_NONE;

	event = &events[id];

	for (i = 0; i < event->count && event->handlers[i].callback != callback; i++);

	if (i == event->count) {
		struct event_handler *eh;

		eh = mem_realloc(event->handlers,
				 (event->count + 1) * sizeof(struct event_handler));

		if (!eh) return EVENT_NONE;

		event->handlers = eh;
		event->count++;
	} else {
		move_event_handler(event, i, i + 1);
	}

	for (i = 0; i < event->count - 1 && priority <= event->handlers[i].priority; i++);

	move_event_handler(event, i + 1, i);

	event->handlers[i].callback = callback;
	event->handlers[i].priority = priority;

	return id;
}

void
unregister_event_hook(int id, int (*callback)(va_list ap))
{
	struct event *event;

	assert(callback);
	if_assert_failed return;

	if (invalid_event_id(id)) return;

	event = &events[id];
	if (event->handlers) {
		register int i;

		for (i = 0; i < event->count; i++) {
			if (event->handlers[i].callback != callback)
				continue;

			move_event_handler(event, i, i + 1);
			event->count--;
			if (!event->count) {
				mem_free(event->handlers);
				event->handlers = NULL;
			} else {
				struct event_handler *eh;

				eh = mem_realloc(event->handlers,
						 event->count * sizeof(struct event_handler));
				if (eh) event->handlers = eh;
			}

			break;
		}
	}
}

void
init_event(void)
{
	event_hash = init_hash(8, strhash);
}

void
done_event(void)
{
	register int i;

	if (event_hash)	free_hash(event_hash);

	for (i = 0; i < eventssize; i++) {
		if (events[i].handlers)
			mem_free(events[i].handlers);
		mem_free(events[i].name);
	}

	if (events) mem_free(events), events = NULL;
	eventssize = 0;
}
