/* $Id: lists.h,v 1.1 2002/06/17 08:00:16 pasky Exp $ */

#ifndef EL__UTIL_LISTS_H
#define EL__UTIL_LISTS_H

struct list_head {
	void *next;
	void *prev;
};

#ifndef HAVE_TYPEOF

struct xlist_head {
	struct xlist_head *next;
	struct xlist_head *prev;
};

#endif

#define init_list(x) \
do { \
	(x).next = &(x); \
	(x).prev = &(x); \
} while (0)

#define list_empty(x) ((x).next == &(x))

#define del_from_list(x) \
do { \
	((struct list_head *) (x)->next)->prev = (x)->prev; \
	((struct list_head *) (x)->prev)->next = (x)->next; \
} while (0)

#define add_at_pos(p,x) \
do { \
	(x)->next = (p)->next; \
	(x)->prev = (p); \
   	(p)->next = (x); \
   	(x)->next->prev = (x); \
} while (0)

#ifdef HAVE_TYPEOF
#define add_to_list(l,x) add_at_pos((typeof(x)) &(l), (x))
#define foreach(e,l) for ((e) = (l).next; (e) != (typeof(e)) &(l); (e) = (e)->next)
#define foreachback(e,l) for ((e) = (l).prev; (e) != (typeof(e)) &(l); (e) = (e)->prev)
#else
#define add_to_list(l,x) add_at_pos((struct xlist_head *) &(l), (struct xlist_head *) (x))
#define foreach(e,l) for ((e) = (l).next; (e) != (void *) &(l); (e) = (e)->next)
#define foreachback(e,l) for ((e) = (l).prev; (e) != (void *) &(l); (e) = (e)->prev)
#endif

#define free_list(l) \
do { \
	while ((l).next != &(l)) { \
		struct list_head *a=(l).next; \
		del_from_list(a); \
		mem_free(a); \
	} \
} while (0)

#endif
