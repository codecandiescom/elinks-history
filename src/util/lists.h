/* $Id: lists.h,v 1.20 2003/05/24 21:08:18 pasky Exp $ */

#ifndef EL__UTIL_LISTS_H
#define EL__UTIL_LISTS_H

#include "util/error.h" /* do_not_optimize_here() */

/* BEWARE! You MAY NOT use ternary operator as parameter to there functions,
 * because they are likely to take & of the parameter. Worst of all, it will
 * work with gcc. But nowhere else (at least not w/ tcc). */

/* Lists debugging
 * Two unsigned int magic number will be put before and after the next and
 * prev pointers, these will be check on list operations.
 * Some pointers are set to specific values after action. */
#ifdef DEBUG
#define LISTDEBUG
#endif



#ifndef LISTDEBUG

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
	(x).next = (x).prev = &(x); \
} while (0)


#define list_empty(x) ((x).next == &(x))

#define del_from_list(x) \
do { \
	do_not_optimize_here(x); \
	((struct list_head *) (x)->next)->prev = (x)->prev; \
	((struct list_head *) (x)->prev)->next = (x)->next; \
	do_not_optimize_here(x); \
} while (0)

#define add_at_pos(p,x) \
do { \
	do_not_optimize_here(p); \
	(x)->next = (p)->next; \
	(x)->prev = (p); \
   	(p)->next = (x); \
   	(x)->next->prev = (x); \
	do_not_optimize_here(p); \
} while (0)


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

#define NULL_LIST_HEAD NULL, NULL
#define D_LIST_HEAD(x) &x, &x
#define INIT_LIST_HEAD(x) struct list_head x = { D_LIST_HEAD(x) }
#define LIST_HEAD(x) x *next; x *prev
#define LIST_SET_MAGIC(x)




#else /* LISTDEBUG */




#define LISTMAGIC1 ((void *) 0xdadababa)
#define LISTMAGIC2 ((void *) 0xd0d0b0b0)

/* I hope #xyz is ANSI C ;-). Or.. oh well, it's just debug :^). --pasky */
#define list_magic_error(where,what) list_magic_error_(where, #what, __FILE__, __LINE__)

struct list_head {
	void *magic1;
	void *next;
	void *prev;
	void *magic2;
};

#ifndef HAVE_TYPEOF
struct xlist_head {
	void *magic1;
	struct xlist_head *next;
	struct xlist_head *prev;
	void *magic2;
};
#endif

#define init_list(x) \
do { \
	(x).magic1 = LISTMAGIC1; \
	(x).magic2 = LISTMAGIC2; \
	(x).next = (x).prev = &(x); \
} while (0)


#define list_empty(x) \
	((((x).magic1 == LISTMAGIC1 && (x).magic2 == LISTMAGIC2) \
	  || (list_magic_error("list_empty", x), 1)) && (x).next == &(x))

#define del_from_list(x) \
do { \
	if ((x)->magic1 != LISTMAGIC1 || (x)->magic2 != LISTMAGIC2) \
		list_magic_error("del_from_list", x); \
	do_not_optimize_here(x); \
	((struct list_head *) (x)->next)->prev = (x)->prev; \
	((struct list_head *) (x)->prev)->next = (x)->next; \
	/* Little hack: we put the complement of LISTMAGIC1 to prev */\
	/* and the line number in next. Debugging purpose. */ \
	(x)->prev = (void *) ~((unsigned int) LISTMAGIC1); \
	(x)->next = (void *)((unsigned int) __LINE__); \
	do_not_optimize_here(x); \
} while (0)

#define add_at_pos(p,x) \
do { \
	if ((p)->magic1 != LISTMAGIC1 || (p)->magic2 != LISTMAGIC2) \
		list_magic_error("add_at_pos", p); \
	do_not_optimize_here(p); \
	(x)->next = (p)->next; \
	(x)->prev = (p); \
   	(p)->next = (x); \
   	(x)->next->prev = (x); \
	(x)->magic1 = LISTMAGIC1; \
	(x)->magic2 = LISTMAGIC2; \
	do_not_optimize_here(p); \
} while (0)


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
	if ((l).magic1 != LISTMAGIC1 || (l).magic2 != LISTMAGIC2) \
		list_magic_error("free_list", &l); \
	do_not_optimize_here(&l); \
	while ((l).next != &(l)) { \
		struct list_head *a__ = (l).next; \
		del_from_list(a__); \
		mem_free(a__); \
	} \
	do_not_optimize_here(&l); \
} while (0)

#define NULL_LIST_HEAD LISTMAGIC1, NULL, NULL, LISTMAGIC2
#define D_LIST_HEAD(x) LISTMAGIC1, &x, &x, LISTMAGIC2
#define INIT_LIST_HEAD(x) struct list_head x = { D_LIST_HEAD(x) }
#define LIST_HEAD(x) void *magic1; x *next; x *prev; void *magic2;
#define LIST_SET_MAGIC(x) do { x->magic1 = LISTMAGIC1; x->magic2 = LISTMAGIC2 } while (0)

#endif /* LISTDEBUG */




#endif /* EL__UTIL_LISTS_H */
