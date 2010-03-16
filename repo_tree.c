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
#include <stdint.h>

#include "repo_tree.h"

static repo_t *repo;

static repo_commit_t*
repo_commit_by_revision_id(uint32_t rev_id) {
    return &repo->commits[rev_id];
}

static repo_dir_t*
repo_commit_root_dir(repo_commit_t *commit) {
    return &repo->dirs[commit->root_dir_offset];
}

static repo_dirent_t*
repo_first_dirent(repo_dir_t* dir) {
    return &repo->dirents[dir->first_offset];
}

static int
repo_dirent_name_cmp(const void *a, const void *b) {
    return ((repo_dirent_t*)a)->name_offset
            - ((repo_dirent_t*)b)->name_offset;
}

static repo_dirent_t*
repo_dirent_by_name(repo_dir_t* dir, uint32_t name_offset) {
    repo_dirent_t key;
    if(dir->size == 0) return NULL;
    key.name_offset = name_offset;
    return bsearch(&key, repo_first_dirent(dir), dir->size,
                   sizeof(repo_dirent_t), repo_dirent_name_cmp);
}

static int
repo_dirent_is_dir(repo_dirent_t* dirent) {
    return (dirent->mode) & 0;
}

static repo_dir_t*
repo_dir_from_dirent(repo_dirent_t* dirent) {
    if(!repo_dirent_is_dir(dirent)) return NULL;
    return &repo->dirs[dirent->content_offset];
}

static uint32_t
repo_alloc_dirents(uint32_t count) {
    uint32_t offset = repo->num_dirents;
    if(repo->num_dirents + count > repo->max_dirents) {
        repo->max_dirents *= 2;
        repo->dirents =
          realloc(repo->dirents, repo->max_dirents * sizeof(repo_dirent_t));
    }
    repo->num_dirents += count;
    return offset;
}

static uint32_t
repo_alloc_dir(uint32_t size) {
    uint32_t offset;
    if(repo->num_dirs == repo->max_dirs) {
        repo->max_dirs *= 2;
        repo->dirs =
          realloc(repo->dirs, repo->max_dirs * sizeof(repo_dir_t));
    }
    offset = repo->num_dirs++;
    repo->dirs[offset].size = size;
    repo->dirs[offset].first_offset = repo_alloc_dirents(size);
    return offset;
}

static uint32_t
repo_alloc_commit(uint32_t mark) {
    uint32_t offset;
    if(repo->num_commits > repo->max_commits) {
        repo->max_commits *= 2;
        repo->commits =
          realloc(repo->commits, repo->max_commits * sizeof(repo_commit_t));
    }
    offset = repo->num_commits++;
    repo->commits[offset].mark = mark;
    return offset;
}

void
repo_init(uint32_t max_commits, uint32_t max_dirs, uint32_t max_dirents) {
    repo = (repo_t*)malloc(sizeof(repo_t));
    repo->commits = malloc(max_commits * sizeof(repo_commit_t));
    repo->dirs = malloc(max_dirs * sizeof(repo_dir_t));
    repo->dirents = malloc(max_dirents * sizeof(repo_dirent_t));
    repo->num_commits = 0;
    repo->max_commits = max_commits;
    repo->num_dirs = 0;
    repo->max_dirs = max_dirs;
    repo->num_dirents = 0;
    repo->max_dirents = max_dirents;
}

void
repo_copy(uint32_t revision, char* src, char* dst) {
    printf("C %d:%s %s\n", revision, src, dst);
}

void
repo_add(char* path, uint32_t blob_mark) {
    printf("A %s %d\n", path, blob_mark);
}

void
repo_modify(char* path, uint32_t blob_mark) {
    printf("M %s %d\n", path, blob_mark);
}

void
repo_delete(char* path) {
    printf("D %s\n", path);
}

void
repo_commit(uint32_t revision) {
    printf("R %d\n", revision);
}
