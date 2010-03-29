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
#include "obj_pool.h"
#include "string_pool.h"

typedef struct node_s node_t;

struct node_s {
    uint32_t offset;
     trp_node(node_t) children;
};

typedef trp(node_t) tree_t;

static tree_t tree = { ~0 };

obj_pool_gen(node, node_t, 4096);
obj_pool_gen(string, char, 4096);

static char *node_value(node_t * node)
{
    return node ? string_pointer(node->offset) : NULL;
}

static int node_value_cmp(node_t * a, node_t * b)
{
    return strcmp(node_value(a), node_value(b));
}

static int node_indentity_cmp(node_t * a, node_t * b)
{
    int r = node_value_cmp(a, b);
    return r ? r : (((uintptr_t) a) > ((uintptr_t) b))
        - (((uintptr_t) a) < ((uintptr_t) b));
}

trp_gen(static, tree_, tree_t, node_t, children, node,
        node_indentity_cmp);

char *pool_fetch(uint32_t entry)
{
    return node_value(node_pointer(entry));
}

uint32_t pool_intern(char *key)
{
    node_t *match = NULL;
    uint32_t key_len = strlen(key) + 1;
    node_t *node = node_pointer(node_alloc(1));
    node->offset = string_alloc(key_len);
    strcpy(node_value(node), key);
    match = tree_psearch(&tree, node);
    if (!match || node_value_cmp(node, match)) {
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

void pool_print_seq(uint32_t len, uint32_t * seq, char delim, FILE * stream)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        fputs(pool_fetch(seq[i]), stream);
        if (i < len - 1)
            fputc(delim, stream);
    }
}

