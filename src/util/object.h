/* $Id: object.h,v 1.7 2004/03/09 12:24:38 jonas Exp $ */

#ifndef EL__UTIL_OBJECT_H
#define EL__UTIL_OBJECT_H

#if 0
#define DEBUG_REFCOUNT
#endif

#ifdef DEBUG_REFCOUNT
#include "util/error.h"
#define object_lock_debug(obj, info) \
	DBG("object %p lock %s now %d", obj, info, (obj)->refcount)
#else
#define object_lock_debug(obj, info)
#endif

#ifdef CONFIG_DEBUG
#include "util/error.h"
#define object_sanity_check(obj)					\
	do {								\
		assert(obj);						\
		assertm((obj)->refcount >= 0,				\
			"Object (%p) refcount underflow.", obj);	\
		if_assert_failed (obj)->refcount = 0;			\
	} while (0)
#else
#define object_sanity_check(doc)
#endif

#define get_object_refcount(obj) ((obj)->refcount)
#define is_object_used(obj) (!!(obj)->refcount)

#define object_lock(obj)						\
	do {								\
		object_sanity_check(obj);				\
		(obj)->refcount++;					\
		object_lock_debug(obj, "+1");				\
	} while (0)

#define object_unlock(obj)						\
	do {								\
		(obj)->refcount--;					\
		object_lock_debug(obj, "-1");				\
		object_sanity_check(obj);				\
	} while (0)

/* Please keep this one. It serves for debugging. --Zas */
#define object_nolock(obj)						\
	do {								\
		object_sanity_check(obj);				\
		object_lock_debug(obj, "0");				\
	} while (0)

#endif
