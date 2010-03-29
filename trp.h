/******************************************************************************
 *
 * Copyright (C) 2008 Jason Evans <jasone@canonware.com>.
 * All rights reserved.
 * 
 * Copyright (C) 2010 David Barr <david.barr@cordelta.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer
 *    unmodified other than the allowable addition of one or more
 *    copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 *
 * cpp macro implementation of treaps.
 *
 * Usage:
 *
 *   (Optional, see assert(3).)
 *   #define NDEBUG
 *
 *   (Required.)
 *   #include <stdint.h> (For uint32_t.)
 *   #include <assert.h>
 *   #include <trp.h>
 *   trp(...)
 *   trp_gen(...)
 *   ...
 *
 ******************************************************************************/

#ifndef TRP_H_
#define	TRP_H_

/* Node structure. */
#define	trp_node(a_type)						\
struct {								\
    uint32_t trpn_left;							\
    uint32_t trpn_right;						\
}

/* Root structure. */
#define	trp(a_type)							\
struct {								\
    uint32_t trp_root;							\
}

/* Pointer/Offset conversion */
#define trpn_pointer(a_base, a_offset)					\
    (a_base##_pointer(a_offset))
#define trpn_offset(a_base, a_pointer)				\
    (a_base##_offset(a_pointer))

/* Left accessors. */
#define	trp_left_get(a_base, a_type, a_field, a_node)			\
    trpn_pointer(a_base, (a_node)->a_field.trpn_left)
#define	trp_left_set(a_base, a_type, a_field, a_node, a_left) do {	\
    (a_node)->a_field.trpn_left = trpn_offset(a_base, a_left);	\
} while (0)

/* Right accessors. */
#define	trp_right_get(a_base, a_type, a_field, a_node)			\
    trpn_pointer(a_base, (a_node)->a_field.trpn_right)
#define	trp_right_set(a_base, a_type, a_field, a_node, a_right) do {	\
    (a_node)->a_field.trpn_right = trpn_offset(a_base, a_right);\
} while (0)

/* Priority accessors. */
#define	trp_prio_get(a_type, a_field, a_node)				\
    (2654435761*(uint32_t)(uintptr_t)(a_node))

/* Node initializer. */
#define	trp_node_new(a_base, a_type, a_field, a_trp, a_node) do {	\
    trp_left_set(a_base, a_type, a_field, (a_node), NULL);		\
    trp_right_set(a_base, a_type, a_field, (a_node), NULL);		\
} while (0)

/* Tree initializer. */
#define	trp_new(a_type, a_base, a_trp) do {			\
    (a_trp)->trp_root = trpn_offset(a_base, NULL);						\
} while (0)

/* Internal utility macros. */
#define	trpn_rotate_left(a_base, a_type, a_field, a_node, r_node) do {	\
    (r_node) = trp_right_get(a_base, a_type, a_field, (a_node));	\
    trp_right_set(a_base, a_type, a_field, (a_node),			\
      trp_left_get(a_base, a_type, a_field, (r_node)));			\
    trp_left_set(a_base, a_type, a_field, (r_node), (a_node));		\
} while (0)

#define	trpn_rotate_right(a_base, a_type, a_field, a_node, r_node) do {	\
    (r_node) = trp_left_get(a_base, a_type, a_field, (a_node));		\
    trp_left_set(a_base, a_type, a_field, (a_node),			\
      trp_right_get(a_base, a_type, a_field, (r_node)));		\
    trp_right_set(a_base, a_type, a_field, (r_node), (a_node));		\
} while (0)

/*
 * The trp_gen() macro generates a type-specific treap implementation,
 * based on the above cpp macros.
 *
 * Arguments:
 *
 *   a_attr     : Function attribute for generated functions (ex: static).
 *   a_pre      : Prefix for generated functions (ex: treap_).
 *   a_t_type   : Type for treap data structure (ex: treap_t).
 *   a_type     : Type for treap node data structure (ex: treap_node_t).
 *   a_field    : Name of treap node linkage (ex: treap_link).
 *   a_base     : Expression for the base pointer from which nodes are offset.
 *   a_cmp      : Node comparison function name, with the following prototype:
 *                  int (a_cmp *)(a_type *a_node, a_type *a_other);
 *                                        ^^^^^^
 *                                     or a_key
 *                Interpretation of comparision function return values:
 *                  -1 : a_node <  a_other
 *                   0 : a_node == a_other
 *                   1 : a_node >  a_other
 *                In all cases, the a_node or a_key macro argument is the first
 *                argument to the comparison function, which makes it possible
 *                to write comparison functions that treat the first argument
 *                specially.
 *
 * Assuming the following setup:
 *
 *   typedef struct ex_node_s ex_node_t;
 *   struct ex_node_s {
 *       trp_node(ex_node_t) ex_link;
 *   };
 *   typedef trp(ex_node_t) ex_t;
 *   static ex_node_t ex_base[MAX_NODES];
 *   trp_gen(static, ex_, ex_t, ex_node_t, ex_link, ex_base, ex_cmp)
 *
 * The following API is generated:
 *
 *   static void
 *   ex_new(ex_t *treap, uint32_t seed);
 *       Description: Initialize a treap structure.
 *       Args:
 *         treap: Pointer to an uninitialized treap object.
 *         seed : Pseudo-random number generator seed.  The seed value isn't
 *                very important, so if in doubt, pick a favorite number.
 *
 *   static ex_node_t *
 *   ex_psearch(ex_t *treap, ex_node_t *key);
 *       Description: Search for node that matches key.  If no match is found,
 *                    return what would be key's successor/predecessor, were
 *                    key in treap.
 *       Args:
 *         treap: Pointer to a initialized treap object.
 *         key  : Search key.
 *       Ret: Node in treap that matches key, or if no match, hypothetical
 *            node's successor/predecessor (NULL if no successor/predecessor).
 *
 *   static void
 *   ex_insert(ex_t *treap, ex_node_t *node);
 *       Description: Insert node into treap.
 *       Args:
 *         treap: Pointer to a initialized treap object.
 *         node : Node to be inserted into treap.
 */
#define	trp_gen(a_attr, a_pre, a_t_type, a_type, a_field, a_base, a_cmp)\
a_attr void								\
a_pre##new(a_t_type *treap, uint32_t seed) {				\
    trp_new(a_type, a_base, treap);				\
}									\
a_attr a_type *								\
a_pre##search(a_t_type *treap, a_type *key) {				\
    a_type *ret;							\
    int cmp;								\
    ret = trpn_pointer(a_base, treap->trp_root);						\
    while (ret != NULL							\
      && (cmp = (a_cmp)(key, ret)) != 0) {				\
	if (cmp < 0) {							\
	    ret = trp_left_get(a_base, a_type, a_field, ret);		\
	} else {							\
	    ret = trp_right_get(a_base, a_type, a_field, ret);		\
	}								\
    }									\
    return (ret);							\
}									\
a_attr a_type *								\
a_pre##psearch(a_t_type *treap, a_type *key) {				\
    a_type *ret;							\
    a_type *tnode = trpn_pointer(a_base, treap->trp_root);					\
    ret = NULL;								\
    while (tnode != NULL) {						\
	int cmp = (a_cmp)(key, tnode);					\
	if (cmp < 0) {							\
	    tnode = trp_left_get(a_base, a_type, a_field, tnode);	\
	} else if (cmp > 0) {						\
	    ret = tnode;						\
	    tnode = trp_right_get(a_base, a_type, a_field, tnode);	\
	} else {							\
	    ret = tnode;						\
	    break;							\
	}								\
    }									\
    return (ret);							\
}									\
a_attr a_type *								\
a_pre##insert_recurse(a_type *cur_node, a_type *ins_node) {		\
    if (cur_node == NULL) {						\
	return (ins_node);						\
    } else {								\
	a_type *ret;							\
	int cmp = a_cmp(ins_node, cur_node);				\
	assert(cmp != 0);						\
	if (cmp < 0) {							\
	    a_type *left = a_pre##insert_recurse(trp_left_get(a_base,	\
              a_type, a_field, cur_node), ins_node);			\
	    trp_left_set(a_base, a_type, a_field, cur_node, left);	\
	    if (trp_prio_get(a_type, a_field, left) <			\
	      trp_prio_get(a_type, a_field, cur_node)) {		\
		trpn_rotate_right(a_base, a_type, a_field, cur_node,	\
                  ret);							\
	    } else {							\
		ret = cur_node;						\
	    }								\
	} else {							\
	    a_type *right = a_pre##insert_recurse(trp_right_get(a_base, \
	      a_type, a_field, cur_node), ins_node);			\
	    trp_right_set(a_base, a_type, a_field, cur_node, right);	\
	    if (trp_prio_get(a_type, a_field, right) <			\
	      trp_prio_get(a_type, a_field, cur_node)) {		\
		trpn_rotate_left(a_base, a_type, a_field, cur_node,	\
                  ret);							\
	    } else {							\
		ret = cur_node;						\
	    }								\
	}								\
	return (ret);							\
    }									\
}									\
a_attr void								\
a_pre##insert(a_t_type *treap, a_type *node) {				\
    trp_node_new(a_base, a_type, a_field, treap, node);			\
    treap->trp_root = trpn_offset(a_base, a_pre##insert_recurse(trpn_pointer(a_base, treap->trp_root), node));	\
}
#endif                          /* TRP_H_ */
