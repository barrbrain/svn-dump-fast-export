/******************************************************************************
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
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define NDEBUG
#include <assert.h>
#include "trp.h"
#include "string_pool.h"

typedef struct node_s node_t;

struct node_s {
    uint32_t offset;
    trp_node(node_t) children;
};

typedef trp(node_t) tree_t;

struct pool_s {
    uint32_t size;
    uint32_t max_size;
    uint32_t entries;
    uint32_t max_entries;
    char *data;
    node_t *index;
    tree_t tree;
};

static struct pool_s *pool;

static char*
node_value(node_t *node) {
    return &(pool->data[node->offset]);
}

static int
node_value_cmp(node_t *a, node_t *b) {
    return strcmp(node_value(a), node_value(b));
}

static int
node_indentity_cmp(node_t *a, node_t *b) {
    int r = node_value_cmp(a, b);
    return r ? r : (((uintptr_t) a) > ((uintptr_t) b))
	  - (((uintptr_t) a) < ((uintptr_t) b));
}

trp_gen(static, tree_, tree_t, node_t, children, pool->index, node_indentity_cmp);

void
pool_init(uint32_t max_size, uint32_t max_entries) {
    pool = malloc(sizeof(*pool));
    pool->data = malloc(max_size);
    pool->size = 0;
    pool->max_size = max_size;
    pool->index = malloc(max_entries * sizeof(node_t));
    // First entry is reserved for NULL
    pool->entries = 1;
    pool->max_entries = max_entries;
    tree_new(&pool->tree, 42);
}

char* 
pool_fetch(uint32_t entry) {
    return node_value(&pool->index[entry]);
}

uint32_t
pool_intern(char* key) {
    uint32_t key_len = strlen(key) + 1;
    node_t *node = NULL;
    node_t *match = NULL;
    if(pool->entries == pool->max_entries) {
        pool->max_entries *= 2; 
        pool->index = realloc(pool->index, pool->max_entries * sizeof(node_t));
    }
    node = &pool->index[pool->entries];
    node->offset = pool->size;
    if(pool->size + key_len > pool->max_size) {
        pool->max_size *= 2;
        pool->data = realloc(pool->data, pool->max_size);
    }
    strcpy(node_value(node), key);
    match = tree_psearch(&pool->tree, node);
    if(!match || node_value_cmp(node, match)) {
       tree_insert(&pool->tree, node);
       pool->size += strlen(key) + 1;
       return pool->entries++;
    } else {
       return match - pool->index;
    }
}

uint32_t
pool_tok_r(char *str, const char *delim, char **saveptr) {
    char * token = strtok_r(str, delim, saveptr);
    return token ? pool_intern(token) : 0;
}
