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

#ifndef REPO_TREE_H_
#define REPO_TREE_H_

#define REPO_MODE_DIR 0040000
#define REPO_MODE_BLB 0100644
#define REPO_MODE_EXE 0100755
#define REPO_MODE_LNK 0120000

typedef struct repo_dirent_s repo_dirent_t;

struct repo_dirent_s {
    uint32_t name_offset;
    uint32_t mode;
    uint32_t content_offset;
};

typedef struct repo_dir_s repo_dir_t;

struct repo_dir_s {
    uint32_t size;
    uint32_t first_offset;
};

typedef struct repo_dir_gc_s repo_dir_gc_t;

struct repo_dir_gc_s {
    uint32_t offset;
    repo_dir_t dir;
};

typedef struct repo_revision_s repo_revision_t;

struct repo_revision_s {
    uint32_t mark;
    uint32_t root_dir_offset;
};

typedef struct repo_s repo_t;

struct repo_s {
    repo_revision_t *commits;
    repo_dir_t *dirs;
    repo_dirent_t *dirents;
    repo_dir_gc_t *gc_dirs;
    uint32_t num_commits;
    uint32_t max_commits;
    uint32_t num_dirs;
    uint32_t num_dirs_saved;
    uint32_t max_dirs;
    uint32_t num_dirents;
    uint32_t num_dirents_saved;
    uint32_t max_dirents;
    uint32_t num_gc_dirs;
    uint32_t max_gc_dirs;
    uint32_t active_commit;
};

void
repo_init(uint32_t max_commits, uint32_t max_dirs,
          uint32_t max_gc_dirs, uint32_t max_dirents);

void repo_copy(uint32_t revision, char *src, char *dst);

void repo_add(char *path, uint32_t mode, uint32_t blob_mark);

void repo_modify(char *path, uint32_t blob_mark);

void repo_delete(char *path);

void repo_commit(uint32_t revision);

void repo_diff(uint32_t r1, uint32_t r2);

#endif
