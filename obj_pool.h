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

#ifndef OBJ_POOL_H_
#define OBJ_POOL_H_

#include <stdint.h>
#include <stdlib.h>

#define obj_pool_gen(pre, obj_t, initial_capacity)                         \
static struct {                                                            \
    uint32_t size;                                                         \
    uint32_t capacity;                                                     \
    obj_t *base;                                                           \
} pre##_pool = { 0, 0, NULL};                                              \
static uint32_t pre##_alloc(uint32_t count)                                \
{                                                                          \
    uint32_t offset;                                                       \
    while (pre##_pool.size + count > pre##_pool.capacity) {                \
        if (pre##_pool.capacity) {                                         \
            pre##_pool.capacity *= 2;                                      \
        } else {                                                           \
            pre##_pool.capacity = initial_capacity;                        \
        }                                                                  \
        pre##_pool.base =                                                  \
            realloc(pre##_pool.base, pre##_pool.capacity * sizeof(obj_t)); \
    }                                                                      \
    offset = pre##_pool.size;                                              \
    pre##_pool.size += count;                                              \
    return offset;                                                         \
}                                                                          \
static void pre##_free(uint32_t count)                                     \
{                                                                          \
    pre##_pool.size -= count;                                              \
}                                                                          \
static uint32_t pre##_offset(obj_t * obj)                                  \
{                                                                          \
    return obj == NULL ? ~0 : obj - pre##_pool.base;                       \
}                                                                          \
static obj_t *pre##_pointer(uint32_t offset)                               \
{                                                                          \
    return offset == ~0 ? NULL : &pre##_pool.base[offset];                 \
}                                                                          \

#endif
