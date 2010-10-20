/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "compat-util.h"
#include "trp.h"
#include "obj_pool.h"
#include "string_pool.h"

static struct trp_root tree = { ~0 };

struct node {
	uint32_t offset;
	struct trp_node children;
};

/* Two memory pools: one for struct node, and another for strings */
obj_pool_gen(node, struct node, 4096);
obj_pool_gen(string, char, 4096);

static char *node_value(struct node *node)
{
	return node ? string_pointer(node->offset) : NULL;
}

static int node_cmp(struct node *a, struct node *b)
{
	return strcmp(node_value(a), node_value(b));
}

/* Build a Treap from the node structure (a trp_node w/ offset) */
trp_gen(static, tree_, struct node, children, node, node_cmp);

char *pool_fetch(uint32_t entry)
{
	return node_value(node_pointer(entry));
}

uint32_t pool_intern(char *key)
{
	/* Canonicalize key */
	struct node *match = NULL;
	uint32_t key_len;
	if (key == NULL)
		return ~0;
	key_len = strlen(key) + 1;
	struct node *node = node_pointer(node_alloc(1));
	node->offset = string_alloc(key_len);
	strcpy(node_value(node), key);
	match = tree_search(&tree, node);
	if (!match) {
		tree_insert(&tree, node);
	} else {
		node_free(1);
		string_free(key_len);
		node = match;
	}
	return node_offset(node);
}

uint32_t pool_tok_r(char *str, const char *delim, char **saveptr)
{
	char *token = strtok_r(str, delim, saveptr);
	return token ? pool_intern(token) : ~0;
}

void pool_print_seq(uint32_t len, uint32_t *seq, char delim, FILE *stream)
{
	uint32_t i;
	for (i = 0; i < len && ~seq[i]; i++) {
		fputs(pool_fetch(seq[i]), stream);
		if (i < len - 1 && ~seq[i + 1])
			fputc(delim, stream);
	}
}

uint32_t pool_tok_seq(uint32_t max, uint32_t *seq, char *delim, char *str)
{
	char *context = NULL;
	uint32_t length = 0, token = str ? pool_tok_r(str, delim, &context) : ~0;
	while (length < max) {
		seq[length++] = token;
		if (token == ~0)
			break;
		token = pool_tok_r(NULL, delim, &context);
	}
	seq[length ? length - 1 : 0] = ~0;
	return length;
}

void pool_init(void)
{
	uint32_t node;
	uint32_t string = 0;
	while (string < string_pool.size) {
		node = node_alloc(1);
		node_pointer(node)->offset = string;
		tree_insert(&tree, node_pointer(node));
		string += strlen(string_pointer(string)) + 1;
	}
}

void pool_commit(void)
{
	string_commit();
}

void pool_reset(void)
{
	node_reset();
	string_reset();
}
