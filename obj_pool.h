/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#ifndef OBJ_POOL_H_
#define OBJ_POOL_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * The obj_pool_gen() macro generates a type-specific memory pool
 * implementation.
 *
 * Arguments:
 *
 *   pre              : Prefix for generated functions (ex: string_).
 *   obj_t            : Type for treap data structure (ex: char).
 *   intial_capacity  : The initial size of the memory pool (ex: 4096).
 *
 */
#define obj_pool_gen(pre, obj_t, initial_capacity) \
static struct { \
	uint32_t size; \
	uint32_t capacity; \
	obj_t *base; \
        FILE *file; \
} pre##_pool = { 0, 0, NULL, NULL}; \
static void pre##_init(void) \
{ \
	struct stat st; \
	size_t ps = sysconf (_SC_PAGESIZE); \
	/* Touch binary file before opening read/write */ \
	pre##_pool.file = fopen(#pre ".bin", "a"); \
	fclose(pre##_pool.file); \
	/* Open, check size, compute capacity */ \
	pre##_pool.file = fopen(#pre ".bin", "r+"); \
	fstat(fileno(pre##_pool.file), &st); \
	pre##_pool.size = st.st_size / sizeof(obj_t); \
	pre##_pool.capacity = ((st.st_size + ps - 1) & ~(ps - 1)) / sizeof(obj_t); \
	if (pre##_pool.capacity < initial_capacity) \
		pre##_pool.capacity = initial_capacity; \
	/* Truncate to calculated capacity and map to VM */ \
	ftruncate(fileno(pre##_pool.file), pre##_pool.capacity * sizeof(obj_t)); \
	pre##_pool.base = mmap(0, pre##_pool.capacity * sizeof(obj_t), \
				PROT_READ | PROT_WRITE, MAP_SHARED, \
				fileno(pre##_pool.file), 0); \
} \
static uint32_t pre##_alloc(uint32_t count) \
{ \
	uint32_t offset; \
	if (pre##_pool.size + count > pre##_pool.capacity) { \
		if (NULL == pre##_pool.base) \
			pre##_init(); \
		fsync(fileno(pre##_pool.file)); \
		munmap(pre##_pool.base, \
			pre##_pool.capacity * sizeof(obj_t)); \
		pre##_pool.base = NULL; \
		while (pre##_pool.size + count > pre##_pool.capacity) \
			if (pre##_pool.capacity) \
				pre##_pool.capacity *= 2; \
			else \
				pre##_pool.capacity = initial_capacity; \
		ftruncate(fileno(pre##_pool.file), \
				pre##_pool.capacity * sizeof(obj_t)); \
		pre##_pool.base = \
			mmap(0, pre##_pool.capacity * sizeof(obj_t), \
				PROT_READ | PROT_WRITE, MAP_SHARED, \
				fileno(pre##_pool.file), 0); \
	} \
	offset = pre##_pool.size; \
	pre##_pool.size += count; \
	return offset; \
} \
static void pre##_free(uint32_t count) \
{ \
	pre##_pool.size -= count; \
} \
static uint32_t pre##_offset(obj_t *obj) \
{ \
	return obj == NULL ? ~0 : obj - pre##_pool.base; \
} \
static obj_t *pre##_pointer(uint32_t offset) \
{ \
	return offset >= pre##_pool.size ? NULL : &pre##_pool.base[offset]; \
} \
static void pre##_reset(void) \
{ \
	if (pre##_pool.base) { \
		fsync(fileno(pre##_pool.file)); \
		munmap(pre##_pool.base, \
			pre##_pool.capacity * sizeof(obj_t)); \
		ftruncate(fileno(pre##_pool.file), \
				pre##_pool.size * sizeof(obj_t)); \
		fclose(pre##_pool.file); \
	} \
	pre##_pool.base = NULL; \
	pre##_pool.size = 0; \
	pre##_pool.capacity = 0; \
	pre##_pool.file = NULL; \
}

#endif
