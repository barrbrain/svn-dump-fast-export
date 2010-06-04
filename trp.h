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
	(trpn_pointer(a_base, a_node)->a_field.trpn_left)
#define trp_left_set(a_base, a_field, a_node, a_left) \
	do { trp_left_get(a_base, a_field, a_node) = (a_left); } while(0)

/* Right accessors. */
#define trp_right_get(a_base, a_field, a_node) \
	(trpn_pointer(a_base, a_node)->a_field.trpn_right)
#define trp_right_set(a_base, a_field, a_node, a_right) \
	do { trp_right_get(a_base, a_field, a_node) = (a_right); } while(0)

/* Priority accessors. */
#define KNUTH_GOLDEN_RATIO_32BIT 2654435761u
#define trp_prio_get(a_node) \
	(KNUTH_GOLDEN_RATIO_32BIT*(a_node))

/* Node initializer. */
#define trp_node_new(a_base, a_field, a_node) \
	trp_left_set(a_base, a_field, (a_node), ~0); \
	trp_right_set(a_base, a_field, (a_node), ~0)

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
	uint32_t ret = ~0; \
	uint32_t tnode = treap->trp_root; \
	while (~tnode) { \
		int cmp = (a_cmp)(key, trpn_pointer(a_base, tnode)); \
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
	return trpn_pointer(a_base, ret); \
} \
a_attr uint32_t a_pre##insert_recurse(uint32_t cur_node, uint32_t ins_node) \
{ \
	if (cur_node == ~0) \
		return (ins_node); \
	else { \
		uint32_t ret; \
		int cmp = (a_cmp)(trpn_pointer(a_base, ins_node), \
					trpn_pointer(a_base, cur_node)); \
		if (cmp < 0) { \
			uint32_t left = a_pre##insert_recurse( \
				trp_left_get(a_base, a_field, cur_node), ins_node); \
			trp_left_set(a_base, a_field, cur_node, left); \
			if (trp_prio_get(left) < trp_prio_get(cur_node)) \
				trpn_rotate_right(a_base, a_field, cur_node, ret); \
			else \
				ret = cur_node; \
		} else { \
			uint32_t right = a_pre##insert_recurse( \
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
	uint32_t offset = trpn_offset(a_base, node); \
	trp_node_new(a_base, a_field, offset); \
	treap->trp_root = a_pre##insert_recurse( treap->trp_root, offset); \
}

#endif
