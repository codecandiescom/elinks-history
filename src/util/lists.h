/* $Id: lists.h,v 1.5 2003/04/24 08:23:40 zas Exp $ */

#ifndef EL__UTIL_LISTS_H
#define EL__UTIL_LISTS_H

#include "util/error.h" /* do_not_optimize_here() */

/* BEWARE! You MAY NOT use ternary operator as parameter to there functions,
 * because they are likely to take & of the parameter. Worst of all, it will
 * work with gcc. But nowhere else (at least not w/ tcc). */

/* Define to activate lists debugging
 * Two unsigned int magic number will be put before and after the next and
 * prev pointers, these will be check on list operations. */
#undef LISTDEBUG

#define LISTMAGIC 0xdadababa

#ifdef LISTDEBUG
#define listmagicerror(x) _listmagicerror(x, __FILE__, __LINE__)
#endif

struct list_head {
#ifdef LISTDEBUG
	unsigned int magic1;
#endif
	void *next;
	void *prev;
#ifdef LISTDEBUG
	unsigned int magic2;
#endif
};

#ifndef HAVE_TYPEOF

struct xlist_head {
#ifdef LISTDEBUG
	unsigned int magic1;
#endif
	struct xlist_head *next;
	struct xlist_head *prev;
#ifdef LISTDEBUG
	unsigned int magic2;
#endif
};

#endif

#ifdef LISTDEBUG
#define init_list(x) \
do { \
	(x).magic1 = LISTMAGIC; \
	(x).magic2 = LISTMAGIC; \
	(x).next = (x).prev = &(x); \
} while (0)
#else
#define init_list(x) \
do { \
	(x).next = (x).prev = &(x); \
} while (0)
#endif


#ifdef LISTDEBUG
#define list_empty(x) ((((x).magic1 == LISTMAGIC && (x).magic2 == LISTMAGIC) || (listmagicerror("list_empty"), 1)) && (x).next == &(x))
#else
#define list_empty(x) ((x).next == &(x))
#endif

#ifdef LISTDEBUG
#define del_from_list(x) \
do { \
	if ((x)->magic1 != LISTMAGIC || (x)->magic2 != LISTMAGIC) listmagicerror("del_from_list"); \
	do_not_optimize_here(x); \
	((struct list_head *) (x)->next)->prev = (x)->prev; \
	((struct list_head *) (x)->prev)->next = (x)->next; \
	do_not_optimize_here(x); \
} while (0)
#else
#define del_from_list(x) \
do { \
	do_not_optimize_here(x); \
	((struct list_head *) (x)->next)->prev = (x)->prev; \
	((struct list_head *) (x)->prev)->next = (x)->next; \
	do_not_optimize_here(x); \
} while (0)
#endif

#ifdef LISTDEBUG
#define add_at_pos(p,x) \
do { \
	if ((p)->magic1 != LISTMAGIC || (p)->magic2 != LISTMAGIC) listmagicerror("add_at_pos"); \
	do_not_optimize_here(p); \
	(x)->next = (p)->next; \
	(x)->prev = (p); \
   	(p)->next = (x); \
   	(x)->next->prev = (x); \
	(x)->magic1 = (x)->magic2 = LISTMAGIC; \
	do_not_optimize_here(p); \
} while (0)
#else
#define add_at_pos(p,x) \
do { \
	do_not_optimize_here(p); \
	(x)->next = (p)->next; \
	(x)->prev = (p); \
   	(p)->next = (x); \
   	(x)->next->prev = (x); \
	do_not_optimize_here(p); \
} while (0)

#endif
#ifdef HAVE_TYPEOF

#define add_to_list(l,x) add_at_pos((typeof(x)) &(l), (x))

#define foreach(e,l) \
	for ((e) = (l).next; \
	     (e) != (typeof(e)) &(l); \
	     (e) = (e)->next)

#define foreachback(e,l) \
	for ((e) = (l).prev; \
	     (e) != (typeof(e)) &(l); \
	     (e) = (e)->prev)

#else

#define add_to_list(l,x) \
	add_at_pos((struct xlist_head *) &(l), (struct xlist_head *) (x))

#define foreach(e,l) \
	for ((e) = (l).next;\
	     (e) != (void *) &(l);\
	     (e) = (e)->next)

#define foreachback(e,l) \
	for ((e) = (l).prev; \
	     (e) != (void *) &(l); \
	     (e) = (e)->prev)

#endif /* HAVE_TYPEOF */

#define free_list(l) \
do { \
	do_not_optimize_here(&l); \
	while ((l).next != &(l)) { \
		struct list_head *a__ = (l).next; \
		del_from_list(a__); \
		mem_free(a__); \
	} \
	do_not_optimize_here(&l); \
} while (0)

#ifdef LISTDEBUG
#define INIT_LIST_HEAD(x) struct list_head x = { LISTMAGIC, &x, &x, LISTMAGIC }
#define D_LIST_HEAD(x) LISTMAGIC, &x, &x, LISTMAGIC
#define NULL_LIST_HEAD LISTMAGIC, NULL, NULL, LISTMAGIC
#define LIST_HEAD(x) unsigned int magic1; x *next; x *prev; unsigned int magic2;
#define LIST_SET_MAGIC(x) x->magic1 = x->magic2 = LISTMAGIC
#else
#define INIT_LIST_HEAD(x) struct list_head x = { &x, &x }
#define D_LIST_HEAD(x) &x, &x
#define NULL_LIST_HEAD NULL, NULL
#define LIST_HEAD(x) x *next; x *prev
#define LIST_SET_MAGIC(x)
#endif


#endif
