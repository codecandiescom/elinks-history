/* Hashing infrastructure */
/* $Id: hash.c,v 1.11 2002/11/29 22:53:20 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "util/hash.h"
#include "util/memory.h"


/* We provide common infrastructure for hashing - each hash consists from one
 * particularly large array full of small lists of keys with same index in the
 * array (same hash value). */
/* Each key and value provided to the hash infrastructure MUST be dynamically
 * allocated, as it's being free()d when entry is destroyed. */

/* TODO: This should be universal, not string-centric. */

#define hash_mask(n) (hash_size(n) - 1)

struct hash *
init_hash(int width, hash_func func)
{
	struct hash *hash = mem_alloc(sizeof(struct hash));
	int i;

	if (!hash) {
		return NULL;
	}

	hash->width = width;
	hash->func = func;

	hash->hash = mem_alloc(hash_size(width) * sizeof(struct list_head));
	if (!hash->hash) {
		mem_free(hash);
		return NULL;
	}

	/* Initialize dummy list_heads */
	for (i = 0; i < hash_size(width); i++)
		init_list(hash->hash[i]);

	return hash;
}

void
free_hash(struct hash *hash)
{
	int i;

	for (i = 0; i < hash_size(hash->width); i++)
		free_list(hash->hash[i]);
	
	mem_free(hash->hash);
	mem_free(hash);
}


/* I've no much idea about what to set here.. I think it doesn't matter much
 * anyway.. ;) --pasky */
#define magic 0xdeadbeef


/* Returns hash_item if ok, NULL if error. */
struct hash_item *
add_hash_item(struct hash *hash, unsigned char *key, unsigned int keylen,
	      void *value)
{
	hash_value hashval;
	struct hash_item *item = mem_alloc(sizeof(struct hash_item));

	if (!item) return NULL;
	
	hashval = hash->func(key, keylen, magic) & hash_mask(hash->width);

	item->key = key;
	item->keylen = keylen;
	item->value = value;

	add_to_list(hash->hash[hashval], item);

	return item;
}

inline struct hash_item *
get_hash_item(struct hash *hash, unsigned char *key, unsigned int keylen)
{
	struct hash_item *item;
	hash_value hashval = hash->func(key, keylen, magic)
			     & hash_mask(hash->width);

	foreach (item, hash->hash[hashval]) {
		if (keylen == item->keylen && !memcmp(key, item->key, keylen))
			return item;
	}

	return NULL;
}

#undef magic

/* If key and/or value were dynamically allocated, think about freeing them.
 * This function doesn't do that. */
void
del_hash_item(struct hash *hash, struct hash_item *item)
{
	if (!item) return;

	del_from_list(item);
	mem_free(item);
}


/* String hashing function follows; it is not written by me, somewhere below
 * are credits. I only hacked it a bit. --pasky */

/* TODO: This is a big CPU hog, in fact:
 *
 *   %   cumulative   self              self     total-----------
 *  time   seconds   seconds    calls  us/call  us/call  name----
 *   6.00      0.35     0.06    10126     5.93     5.93  strhash
 *
 * It should be investigated whether we couldn't push this down a little. */

/*
 *  --------------------------------------------------------------------
 *   mix -- mix 3 32-bit values reversibly.
 *   For every delta with one or two bits set, and the deltas of all three
 *     high bits or all three low bits, whether the original value of a,b,c
 *     is almost all zero or is uniformly distributed,
 *   * If mix() is run forward or backward, at least 32 bits in a,b,c
 *     have at least 1/4 probability of changing.
 *   * If mix() is run forward, every bit of c will change between 1/3 and
 *     2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
 *     mix() was built out of 36 single-cycle latency instructions in a
 *     structure that could supported 2x parallelism, like so:
 *       a -= b;
 *       a -= c; x = (c>>13);
 *       b -= c; a ^= x;
 *       b -= a; x = (a<<8);
 *       c -= a; b ^= x;
 *       c -= b; x = (b>>13);
 *       ...
 *     Unfortunately, superscalar Pentiums and Sparcs can't take advantage
 *     of that parallelism.  They've also turned some of those single-cycle
 *     latency instructions into multi-cycle latency instructions.  Still,
 *     this is the fastest good hash I could find.  There were about 2^^68
 *     to choose from.  I only looked at a billion or so.
 *  --------------------------------------------------------------------
*/

#define mix(a, b, c) { \
	a -= b; a -= c; a ^= (c>>13); \
	b -= c; b -= a; b ^= (a<<8); \
	c -= a; c -= b; c ^= (b>>13); \
	a -= b; a -= c; a ^= (c>>12);  \
	b -= c; b -= a; b ^= (a<<16); \
	c -= a; c -= b; c ^= (b>>5); \
	a -= b; a -= c; a ^= (c>>3);  \
	b -= c; b -= a; b ^= (a<<10); \
	c -= a; c -= b; c ^= (b>>15); \
}

 /*
 --------------------------------------------------------------------
 hash() -- hash a variable-length key into a 32-bit value
   k       : the key (the unaligned variable-length array of bytes)
   len     : the length of the key, counting by bytes
   initval : can be any 4-byte value
 Returns a 32-bit value.  Every bit of the key affects every bit of
 the return value.  Every 1-bit and 2-bit delta achieves avalanche.
 About 6*len+35 instructions.

 The best hash table sizes are powers of 2.  There is no need to do
 mod a prime (mod is sooo slow!).  If you need less than 32 bits,
 use a bitmask.  For example, if you need only 10 bits, do
   h = (h & hashmask(10));
 In which case, the hash table should have hashsize(10) elements.

 If you are hashing n strings (ub1 **)k, do it like this:
   for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

 By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
 code any way you wish, private, educational, or commercial.  It's free.

 See http://burtleburtle.net/bob/hash/evahash.html
 Use for hash table lookup, or anything where one collision in 2^^32 is
 acceptable.  Do NOT use for cryptographic purposes.
 --------------------------------------------------------------------
 */

#define keycompute(a) ((k[(a)]) \
			+ ((hash_value)(k[(a)+1])<<8) \
			+ ((hash_value)(k[(a)+2])<<16) \
			+ ((hash_value)(k[(a)+3])<<24))

hash_value
strhash(unsigned char *k, /* the key */
	unsigned int length, /* the length of the key */
	hash_value initval /* the previous hash, or an arbitrary value */)
{
	register int len;
	register hash_value a, b, c;

	/* Set up the internal state */
	len = length;
	a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
	c = initval;         /* the previous hash value */

	/*---------------------------------------- handle most of the key */
	while (len >= 12) {
		a += keycompute(0);
		b += keycompute(4);
		c += keycompute(8);
		mix(a, b, c);
		k += 12;
		len -= 12;
	}

	/*------------------------------------- handle the last 11 bytes */
	c += length;
	switch(len) {	/* all the case statements fall through */
		case 11: c += ((hash_value)(k[10])<<24);
		case 10: c += ((hash_value)(k[9])<<16);
		case 9 : c += ((hash_value)(k[8])<<8);
			/* the first byte of c is reserved for the length */
		case 8 : b += ((hash_value)(k[7])<<24);
		case 7 : b += ((hash_value)(k[6])<<16);
		case 6 : b += ((hash_value)(k[5])<<8);
		case 5 : b += (k[4]);
		case 4 : a += ((hash_value)(k[3])<<24);
		case 3 : a += ((hash_value)(k[2])<<16);
		case 2 : a += ((hash_value)(k[1])<<8);
		case 1 : a += (k[0]);
			/* case 0: nothing left to add */
	}

	mix(a, b, c);

	/*-------------------------------------------- report the result */
	return c;
}

#undef keycompute
#undef mix
#undef hash_mask
