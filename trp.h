/*
 * cpp macro implementation of treaps.
 *
 * Usage:
 *   #include <stdint.h>
 *   #include <trp.h>
 *   trp_gen(...)
 *
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#ifndef TRP_H_
#define TRP_H_

/* Node structure. */
struct trp_node {
	uint32_t trpn_left;
	uint32_t trpn_right;
};

/* Root structure. */
struct trp_root {
	uint32_t trp_root;
};

/* Pointer/Offset conversion */
#define trpn_pointer(a_base, a_offset) (a_base##_pointer(a_offset))
#define trpn_offset(a_base, a_pointer) (a_base##_offset(a_pointer))

/* Left accessors. */
#define trp_left_get(a_base, a_field, a_node) \
	trpn_pointer(a_base, (a_node)->a_field.trpn_left)
#define trp_left_set(a_base, a_field, a_node, a_left) \
	(a_node)->a_field.trpn_left = trpn_offset(a_base, a_left)

/* Right accessors. */
#define trp_right_get(a_base, a_field, a_node) \
	trpn_pointer(a_base, (a_node)->a_field.trpn_right)
#define trp_right_set(a_base, a_field, a_node, a_right) \
	(a_node)->a_field.trpn_right = trpn_offset(a_base, a_right)

/* Priority accessors. */
#define KNUTH_GOLDEN_RATIO_32BIT 2654435761u
#define trp_prio_get(a_node) \
	(KNUTH_GOLDEN_RATIO_32BIT*(uint32_t)(uintptr_t)(a_node))

/* Node initializer. */
#define trp_node_new(a_base, a_field, a_node) \
	trp_left_set(a_base, a_field, (a_node), NULL); \
	trp_right_set(a_base, a_field, (a_node), NULL)

/* Internal utility macros. */
#define trpn_rotate_left(a_base, a_field, a_node, r_node) \
	do { (r_node) = trp_right_get(a_base, a_field, (a_node)); \
	trp_right_set(a_base, a_field, (a_node), \
		trp_left_get(a_base, a_field, (r_node))); \
	trp_left_set(a_base, a_field, (r_node), (a_node)); } while(0)

#define trpn_rotate_right(a_base, a_field, a_node, r_node) \
	do { (r_node) = trp_left_get(a_base, a_field, (a_node)); \
	trp_left_set(a_base, a_field, (a_node), \
		trp_right_get(a_base, a_field, (r_node))); \
	trp_right_set(a_base, a_field, (r_node), (a_node)); } while(0)

#define trp_gen(a_attr, a_pre, a_type, a_field, a_base, a_cmp) \
a_attr a_type *a_pre##psearch(struct trp_root *treap, a_type *key) \
{ \
	a_type *ret; \
	a_type *tnode = trpn_pointer(a_base, treap->trp_root); \
	ret = NULL; \
	while (tnode != NULL) { \
		int cmp = (a_cmp)(key, tnode); \
		if (cmp < 0) \
			tnode = trp_left_get(a_base, a_field, tnode); \
		else if (cmp > 0) { \
			ret = tnode; \
			tnode = trp_right_get(a_base, a_field, tnode); \
		} else { \
			ret = tnode; \
			break; \
		} \
	} \
	return (ret); \
} \
a_attr a_type *a_pre##insert_recurse(a_type *cur_node, a_type *ins_node) \
{ \
	if (cur_node == NULL) \
		return (ins_node); \
	else { \
		a_type *ret; \
		int cmp = a_cmp(ins_node, cur_node); \
		if (cmp < 0) { \
			a_type *left = a_pre##insert_recurse( \
				trp_left_get(a_base, a_field, cur_node), ins_node); \
			trp_left_set(a_base, a_field, cur_node, left); \
			if (trp_prio_get(left) < trp_prio_get(cur_node)) \
				trpn_rotate_right(a_base, a_field, cur_node, ret); \
			else \
				ret = cur_node; \
		} else { \
			a_type *right = a_pre##insert_recurse( \
				trp_right_get(a_base, a_field, cur_node), ins_node); \
			trp_right_set(a_base, a_field, cur_node, right); \
			if (trp_prio_get(right) < trp_prio_get(cur_node)) \
				trpn_rotate_left(a_base, a_field, cur_node, ret); \
			else \
				ret = cur_node; \
		} \
		return (ret); \
	} \
} \
a_attr void a_pre##insert(struct trp_root *treap, a_type *node) \
{ \
	trp_node_new(a_base, a_field, node); \
	treap->trp_root = trpn_offset(a_base, a_pre##insert_recurse( \
					      trpn_pointer(a_base, treap->trp_root), \
					      node)); \
}

#endif
