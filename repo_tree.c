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

#include <string.h>
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
    return dirent->mode == REPO_MODE_DIR;
}

static int
repo_dirent_is_blob(repo_dirent_t* dirent) {
    return dirent->mode == REPO_MODE_BLB || dirent->mode == REPO_MODE_EXE;
}

static int
repo_dirent_is_executable(repo_dirent_t* dirent) {
    return dirent->mode == REPO_MODE_EXE;
}

static int
repo_dirent_is_symlink(repo_dirent_t* dirent) {
    return dirent->mode == REPO_MODE_LNK;
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
    repo->active_commit = 1;
    repo_commit_by_revision_id(0)->root_dir_offset = repo_alloc_dir(0);
    repo->num_dirs_saved = repo->num_dirs;
}

static repo_dir_t*
repo_clone_dir(repo_dir_t* orig_dir, uint32_t padding) {
    uint32_t orig_offset, new_offset, dirent_offset;
    orig_offset = orig_dir - repo->dirs;
    if(orig_offset < repo->num_dirs_saved) {
        new_offset = repo_alloc_dir(orig_dir->size + padding);
        orig_dir = &repo->dirs[orig_offset];
        dirent_offset = repo->dirs[new_offset].first_offset;
        printf("Creating dir clone.\n");
    } else {
        if(padding == 0) return orig_dir;
        new_offset = orig_offset;
        dirent_offset = repo_alloc_dirents(orig_dir->size + padding);
        printf("Reallocating dir clone.\n");
    }
    printf("Copying %d entries.\n", orig_dir->size);
    memcpy(&repo->dirents[dirent_offset], repo_first_dirent(orig_dir),
        orig_dir->size * sizeof(repo_dirent_t));
    if(orig_offset >= repo->num_dirs_saved) {
       // bzero(repo_first_dirent(orig_dir),
       //     orig_dir->size * sizeof(repo_dirent_t));
    }
   // bzero(&repo->dirents[dirent_offset + orig_dir->size],
   //     padding * sizeof(repo_dirent_t));
    repo->dirs[new_offset].size = orig_dir->size + padding; 
    repo->dirs[new_offset].first_offset = dirent_offset;
    return &repo->dirs[new_offset];
}

static void
repo_print_tree(uint32_t depth, repo_dir_t* dir) {
    uint32_t i,j;
    repo_dirent_t* dirent;
    for(j=0;j<dir->size;j++) {
        for(i=0;i<depth;i++) putchar(' ');
        dirent = &repo_first_dirent(dir)[j];
        printf("%d\n",dirent->name_offset);
        if(repo_dirent_is_dir(dirent)) {
            repo_print_tree(depth+1, repo_dir_from_dirent(dirent));
        }
    }
}

static repo_dirent_t*
repo_read_dirent(uint32_t revision, char* path) {
    char *ctx;
    uint32_t name;
    repo_dir_t* dir;
    repo_dirent_t* dirent;
    dir = repo_commit_root_dir(repo_commit_by_revision_id(revision));
    if(dir == NULL) {
        printf("No root dir.");
        return NULL;
    }
    for(name = pool_tok_r(path, "/", &ctx);
        name; name = pool_tok_r(NULL, "/", &ctx)) {
        printf("Descending: %d\n", (name));
        dirent = repo_dirent_by_name(dir, name);
        if(dirent == NULL) {
            printf("Not found.");
            return NULL;
        } else if(repo_dirent_is_dir(dirent)) {
            dir = repo_dir_from_dirent(dirent);
            printf("dirent: %d, %07o, %d\n",
                dirent->name_offset, dirent->mode, dirent->content_offset);
        } else {
            printf("Not a directory: %d, %07o, %d\n",
                dirent->name_offset, dirent->mode, dirent->content_offset);
            break;
        }
    }
    return name ? NULL : dirent;
}

static void
repo_write_dirent(char* path, uint32_t mode, uint32_t content_offset) {
    char *ctx;
    uint32_t name, revision, dirent_offset, dir_offset;
    repo_dir_t* dir;
    repo_dirent_t* dirent = NULL;
    revision = repo->active_commit;
    dir = repo_commit_root_dir(repo_commit_by_revision_id(revision));
    dir = repo_clone_dir(dir, 0);
    repo_commit_by_revision_id(revision)->root_dir_offset = dir - repo->dirs;
    for(name = pool_tok_r(path, "/", &ctx);
        name; name = pool_tok_r(NULL, "/", &ctx)) {
        repo_print_tree(0, repo_commit_root_dir(repo_commit_by_revision_id(revision)));
        printf("dir[%d]: %d, %d\n", dir - repo->dirs, dir->size, dir->first_offset);
        printf("Descending: %d\n", (name));
        dirent = repo_dirent_by_name(dir, name);
        if(dirent == NULL) {
            /* Add entry to dir */
            printf("Adding new entry.\n");
            dir = repo_clone_dir(dir, 1);
            printf("dir[%d]: %d, %d\n", dir - repo->dirs, dir->size, dir->first_offset);
            dirent = &repo_first_dirent(dir)[dir->size -1];
            dirent->name_offset = name;
            dirent->mode = REPO_MODE_BLB;
            dirent->content_offset = 0;
            printf("dirent[%d]: %d, %06o, %d\n", dirent - repo->dirents,
                dirent->name_offset, dirent->mode, dirent->content_offset);
            if(*ctx /* not last name */) {
                /* Allocate new directory */
                printf("Populating entry with new directory.\n");
                dirent->mode = REPO_MODE_DIR;
                dir_offset = repo_alloc_dir(0);
                dirent->content_offset = dir_offset;
                dir = &repo->dirs[dir_offset];
                printf("dirent[%d]: %d, %06o, %d\n", dirent - repo->dirents,
                    dirent->name_offset, dirent->mode, dirent->content_offset);
            }
            qsort(repo_first_dirent(dir), dir->size,
                sizeof(repo_dirent_t), repo_dirent_name_cmp);
        } else if(dir = repo_dir_from_dirent(dirent)) {
            /* Clone dir for write */
            printf("Entering existing directory.\n");
            printf("dirent[%d]: %d, %06o, %d\n", dirent - repo->dirents,
                dirent->name_offset, dirent->mode, dirent->content_offset);
            dirent_offset = dirent ? dirent - repo->dirents : ~0;
            dir = repo_clone_dir(dir, 0);
            if(dirent_offset != ~0)
                repo->dirents[dirent_offset].content_offset = dir - repo->dirs;
        } else if(*ctx /* not last name */) {
            /* Allocate new directory */
            printf("Overwriting entry with new directory.\n");
            dirent->mode = REPO_MODE_DIR;
            dirent_offset = dirent - repo->dirents;
            dir_offset = repo_alloc_dir(0);
            dirent = &repo->dirents[dirent_offset];
            dir = &repo->dirs[dir_offset];
            dirent->content_offset = dir_offset;
            printf("dirent[%d]: %d, %06o, %d\n", dirent - repo->dirents,
                dirent->name_offset, dirent->mode, dirent->content_offset);
        }
    }
    dirent->mode = mode;
    dirent->content_offset = content_offset;
    qsort(repo_first_dirent(dir), dir->size,
        sizeof(repo_dirent_t), repo_dirent_name_cmp);
}

void
repo_copy(uint32_t revision, char* src, char* dst) {
    repo_dirent_t *src_dirent;
    printf("C %d:%s %s\n", revision, src, dst);
    src_dirent = repo_read_dirent(revision, src);
    if(src_dirent == NULL) return;
    repo_write_dirent(dst, src_dirent->mode, src_dirent->content_offset);
    repo_print_tree(0, repo_commit_root_dir(repo_commit_by_revision_id(repo->active_commit)));
}

void
repo_add(char* path, uint32_t blob_mark) {
    printf("A %s %d\n", path, blob_mark);
    repo_write_dirent(path, REPO_MODE_BLB, blob_mark);
    repo_print_tree(0, repo_commit_root_dir(repo_commit_by_revision_id(repo->active_commit)));
}

void
repo_modify(char* path, uint32_t blob_mark) {
    printf("M %s %d\n", path, blob_mark);
    repo_write_dirent(path, REPO_MODE_BLB, blob_mark);
    repo_print_tree(0, repo_commit_root_dir(repo_commit_by_revision_id(repo->active_commit)));
}

void
repo_delete(char* path) {
    printf("D %s\n", path);
    repo_print_tree(0, repo_commit_root_dir(repo_commit_by_revision_id(repo->active_commit)));
}

void
repo_commit(uint32_t revision) {
    if(revision == 0) return;
    printf("R %d\n", revision);
    /* compact dirents */
    repo->num_dirs_saved = repo->num_dirs;
    repo->active_commit++;
    repo_alloc_commit(repo->active_commit);
    repo_commit_by_revision_id(repo->active_commit)->root_dir_offset =
        repo_commit_by_revision_id(repo->active_commit - 1)->root_dir_offset;
    printf("Number of dirs: %d\n", repo->num_dirs);
    printf("Number of dirents: %d\n", repo->num_dirents);
}
