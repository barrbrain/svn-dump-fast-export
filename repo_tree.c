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

#include "string_pool.h"
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
    return (((repo_dirent_t*)a)->name_offset
            > ((repo_dirent_t*)b)->name_offset) -
           (((repo_dirent_t*)a)->name_offset
            < ((repo_dirent_t*)b)->name_offset);
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
repo_alloc_gc_dir(void) {
    uint32_t offset;
    if(repo->num_gc_dirs == repo->max_gc_dirs) {
        repo->max_gc_dirs *= 2;
        repo->gc_dirs =
          realloc(repo->gc_dirs, repo->max_gc_dirs * sizeof(repo_dir_gc_t));
    }
    offset = repo->num_gc_dirs++;
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
repo_init(uint32_t max_commits, uint32_t max_dirs,
    uint32_t max_gc_dirs, uint32_t max_dirents) {
    repo = (repo_t*)malloc(sizeof(repo_t));
    repo->commits = malloc(max_commits * sizeof(repo_commit_t));
    repo->dirs = malloc(max_dirs * sizeof(repo_dir_t));
    repo->gc_dirs = malloc(max_gc_dirs * sizeof(repo_dir_gc_t));
    repo->dirents = malloc(max_dirents * sizeof(repo_dirent_t));
    repo->num_commits = 0;
    repo->max_commits = max_commits;
    repo->num_dirs = 0;
    repo->max_dirs = max_dirs;
    repo->num_gc_dirs = 0;
    repo->max_gc_dirs = max_gc_dirs;
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
    } else {
        if(padding == 0) return orig_dir;
        new_offset = orig_offset;
        dirent_offset = repo_alloc_dirents(orig_dir->size + padding);
    }
    memcpy(&repo->dirents[dirent_offset], repo_first_dirent(orig_dir),
        orig_dir->size * sizeof(repo_dirent_t));
    if(orig_offset >= repo->num_dirs_saved) {
        bzero(repo_first_dirent(orig_dir),
            orig_dir->size * sizeof(repo_dirent_t));
    }
    bzero(&repo->dirents[dirent_offset + orig_dir->size],
        padding * sizeof(repo_dirent_t));
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
    for(name = pool_tok_r(path, "/", &ctx);
        name; name = pool_tok_r(NULL, "/", &ctx)) {
        dirent = repo_dirent_by_name(dir, name);
        if(dirent == NULL) {
            return NULL;
        } else if(repo_dirent_is_dir(dirent)) {
            dir = repo_dir_from_dirent(dirent);
        } else {
            break;
        }
    }
    return dirent;
}

static void
repo_write_dirent(char* path, uint32_t mode, uint32_t content_offset, uint32_t del) {
    char *ctx, *end;
    uint32_t name, revision, dirent_offset, dir_offset, p_dir_offset;
    repo_dir_t* dir;
    repo_dirent_t* dirent = NULL;
    end = path + strlen(path);
    revision = repo->active_commit;
    dir = repo_commit_root_dir(repo_commit_by_revision_id(revision));
    dir = repo_clone_dir(dir, 0);
    repo_commit_by_revision_id(revision)->root_dir_offset = dir - repo->dirs;
    for(name = pool_tok_r(path, "/", &ctx);
        name; name = pool_tok_r(NULL, "/", &ctx)) {
        p_dir_offset = dir - repo->dirs;
        dirent = repo_dirent_by_name(dir, name);
        if(dirent == NULL) {
            dir = repo_clone_dir(dir, 1);
            dirent = &repo_first_dirent(dir)[dir->size -1];
            dirent->name_offset = name;
            dirent->mode = REPO_MODE_DIR;
            qsort(repo_first_dirent(dir), dir->size,
                sizeof(repo_dirent_t), repo_dirent_name_cmp);
            dirent = repo_dirent_by_name(dir, name);
            dir_offset = repo_alloc_dir(0);
            dirent->content_offset = dir_offset;
            dir = &repo->dirs[dir_offset];
        } else if(dir = repo_dir_from_dirent(dirent)) {
            dirent_offset = dirent ? dirent - repo->dirents : ~0;
            dir = repo_clone_dir(dir, 0);
            if(dirent_offset != ~0)
                repo->dirents[dirent_offset].content_offset = dir - repo->dirs;
        } else {
            dirent->mode = REPO_MODE_DIR;
            dirent_offset = dirent - repo->dirents;
            dir_offset = repo_alloc_dir(0);
            dirent = &repo->dirents[dirent_offset];
            dir = &repo->dirs[dir_offset];
            dirent->content_offset = dir_offset;
        }
    }
    if(del) dirent->name_offset = ~0;
    dirent->mode = mode;
    dirent->content_offset = content_offset;
    if(del) {
        dir = &repo->dirs[p_dir_offset];
        qsort(repo_first_dirent(dir), dir->size,
            sizeof(repo_dirent_t), repo_dirent_name_cmp);
        dir->size--;
    }
}

void
repo_copy(uint32_t revision, char* src, char* dst) {
    repo_dirent_t *src_dirent;
    printf("C %d:%s %s\n", revision, src, dst);
    src_dirent = repo_read_dirent(revision, src);
    if(src_dirent == NULL) return;
    repo_write_dirent(dst, src_dirent->mode, src_dirent->content_offset, 0);
}

void
repo_add(char* path, uint32_t blob_mark) {
    printf("A %s %d\n", path, blob_mark);
    repo_write_dirent(path, REPO_MODE_BLB, blob_mark, 0);
}

void
repo_modify(char* path, uint32_t blob_mark) {
    printf("M %s %d\n", path, blob_mark);
    repo_write_dirent(path, REPO_MODE_BLB, blob_mark, 0);
}

void
repo_delete(char* path) {
    printf("D %s\n", path);
    repo_write_dirent(path, 0, 0, 1);
}

static void
repo_gc_mark_dirs(repo_dir_t* dir) {
    uint32_t i,j,offset;
    repo_dirent_t* dirent;
    if(dir->size) {
        offset = repo_alloc_gc_dir();
        repo->gc_dirs[offset].offset = dir - repo->dirs;
        repo->gc_dirs[offset].dir = *dir;
    }
    for(j=0;j<dir->size;j++) {
        dirent = &repo_first_dirent(dir)[j];
        if(repo_dirent_is_dir(dirent) &&
            dirent->content_offset >= repo->num_dirs_saved) {
            repo_gc_mark_dirs(repo_dir_from_dirent(dirent));
        }
    }
}

static int
repo_gc_dir_offset_cmp(const void *a, const void *b) {
    return (((repo_dir_gc_t*)a)->dir.first_offset
            > ((repo_dir_gc_t*)b)->dir.first_offset) -
           (((repo_dir_gc_t*)a)->dir.first_offset
            < ((repo_dir_gc_t*)b)->dir.first_offset);
}

static int
repo_gc_dir_src_cmp(const void *a, const void *b) {
    return (((repo_dir_gc_t*)a)->offset
            > ((repo_dir_gc_t*)b)->offset) -
           (((repo_dir_gc_t*)a)->offset
            < ((repo_dir_gc_t*)b)->offset);
}

static repo_dir_gc_t*
repo_gc_find_by_src(uint32_t offset) {
    uint32_t i;
    for(i = 0; i < repo->num_gc_dirs; i++) {
        if(repo->gc_dirs[i].offset == offset) {
            return &repo->gc_dirs[i];
        }
    }
    return NULL;
}

static void
repo_gc_dirents(void) {
    repo_dir_gc_t* gc_dir;
    uint32_t i;
    uint32_t offset = repo->num_dirents_saved;
    for(i=0; i < repo->num_gc_dirs; i++) {
        gc_dir = &repo->gc_dirs[i];
        memmove(&repo->dirents[offset],
            &repo->dirents[gc_dir->dir.first_offset],
            gc_dir->dir.size * sizeof(repo_dirent_t));
        gc_dir->dir.first_offset = offset;
        offset += gc_dir->dir.size;
    }
    repo->num_dirents = offset;
}

static void
repo_gc_dirs(void) {
    uint32_t i;
    repo_dir_gc_t* gc_dir;
    repo_commit_t* commit = repo_commit_by_revision_id(repo->active_commit);
    repo_dir_t* root = repo_commit_root_dir(commit);
    repo->num_gc_dirs = 0;
    repo_gc_mark_dirs(root);
    qsort(repo->gc_dirs, repo->num_gc_dirs, sizeof(repo_dir_gc_t),
        repo_gc_dir_offset_cmp);
    repo_gc_dirents();
    gc_dir = repo_gc_find_by_src(commit->root_dir_offset);
    printf("Remapping commit root: %d => ", commit->root_dir_offset);
    commit->root_dir_offset =
        gc_dir == NULL ? 0 :
        ((gc_dir - repo->gc_dirs) + repo->num_dirs_saved);
    printf("%d\n", commit->root_dir_offset);
    for(i=repo->num_dirents_saved; i < repo->num_dirents; i++) {
        if(repo_dirent_is_dir(&repo->dirents[i]) &&
            repo->dirents[i].content_offset >= repo->num_dirs_saved) {
            gc_dir = repo_gc_find_by_src(repo->dirents[i].content_offset);
            if(gc_dir) {
                repo->dirents[i].content_offset = (gc_dir - repo->gc_dirs) +
                    repo->num_dirs_saved;
            } else {
                repo->dirents[i].content_offset = 0;
            }
        }
    }
    for(i=0; i < repo->num_gc_dirs; i++) {
        repo->dirs[repo->num_dirs_saved + i] = 
            repo->gc_dirs[i].dir;
    }
    repo->num_dirs = repo->num_dirs_saved + repo->num_gc_dirs;
}

void
repo_diff(uint32_t r1, uint32_t r2);

void
repo_commit(uint32_t revision) {
    if(revision == 0) return;
    printf("R %d\n", revision);
    printf("Number of new dirs before GC: %d\n", repo->num_dirs -
        repo->num_dirs_saved);
    printf("Number of new dirents before GC: %d\n", repo->num_dirents-
        repo->num_dirents_saved);
    repo_gc_dirs();
    printf("Number of new dirs: %d\n", repo->num_dirs -
        repo->num_dirs_saved);
    printf("Number of new dirents: %d\n", repo->num_dirents-
        repo->num_dirents_saved);
    /* repo_print_tree(0, repo_commit_root_dir(repo_commit_by_revision_id(repo->active_commit))); */
    repo->num_dirs_saved = repo->num_dirs;
    repo->num_dirents_saved = repo->num_dirents;
    repo_diff(repo->active_commit-1, repo->active_commit);
    repo->active_commit++;
    repo_alloc_commit(repo->active_commit);
    repo_commit_by_revision_id(repo->active_commit)->root_dir_offset =
        repo_commit_by_revision_id(repo->active_commit - 1)->root_dir_offset;
    printf("Number of commits: %d (%dB)\n",
        repo->num_commits, repo->num_commits * sizeof(repo_commit_t));
    printf("Number of dirs: %d (%dB)\n",
        repo->num_dirs, repo->num_dirs * sizeof(repo_dir_t));
    printf("Number of dirents: %d (%dB)\n",
        repo->num_dirents, repo->num_dirents * sizeof(repo_dirent_t));
}

static void
repo_print_path(uint32_t depth, uint32_t* path) {
    uint32_t p;
    for(p=0;p<depth;p++) {
        fputs(pool_fetch(path[p]), stdout);
        if(p<depth-1) putchar('/');
    }
}

static void
repo_git_delete(uint32_t depth, uint32_t* path) {
    putchar('D');
    putchar(' ');
    repo_print_path(depth, path);
    putchar('\n');
}

static void
repo_git_add_r(uint32_t depth, uint32_t* path, repo_dir_t* dir);

static void
repo_git_add(uint32_t depth, uint32_t* path, repo_dirent_t* dirent) {
    if(repo_dirent_is_dir(dirent)) {
        repo_git_add_r(depth, path, repo_dir_from_dirent(dirent));
    } else {
        printf("M %06o :%d ", dirent->mode, dirent->content_offset);    
        repo_print_path(depth, path);
        putchar('\n');
    }
}

static void
repo_git_add_r(uint32_t depth, uint32_t* path, repo_dir_t* dir) {
    uint32_t o;
    repo_dirent_t *de;
    de = repo_first_dirent(dir);
    for(o = 0; o < dir->size; o ++) {
        path[depth] = de[o].name_offset;
        repo_git_add(depth + 1, path, &de[o]);
    }
}

static void
repo_diff_r(uint32_t depth, uint32_t* path, repo_dir_t* dir1, repo_dir_t* dir2) {
    uint32_t o1, o2, p;
    repo_dirent_t *de1, *de2;
    de1 = repo_first_dirent(dir1);
    de2 = repo_first_dirent(dir2);
    for(o1 = o2 = 0; o1 < dir1->size && o2 < dir2->size;) {
       if(de1[o1].name_offset < de2[o2].name_offset) {
           /* delete(o1) */
           path[depth] = de1[o1].name_offset;
           repo_git_delete(depth + 1, path);
           o1++;
       } else if(de1[o1].name_offset == de2[o2].name_offset) {
           path[depth] = de1[o1].name_offset;
           if(de1[o1].content_offset != de2[o2].content_offset) {
               if(repo_dirent_is_dir(&de1[o1]) && repo_dirent_is_dir(&de2[o2])) {
                   /* recursive diff */
                   repo_diff_r(depth + 1, path, repo_dir_from_dirent(&de1[o1]),
                       repo_dir_from_dirent(&de2[o2]));
               } else {
                   /* delete o1, add o2 */
                   if(repo_dirent_is_dir(&de1[o1]) != repo_dirent_is_dir(&de2[o2])) {
                       repo_git_delete(depth + 1, path);
                   }
                   repo_git_add(depth + 1, path, &de2[o2]);
               }
           }
           o1++;
           o2++;
       } else {
           /* add(o2) */
           path[depth] = de2[o2].name_offset;
           repo_git_add(depth + 1, path, &de2[o2]);
           o2++;
       }
    }
    for(; o1 < dir1->size; o1++) {
           /* delete(o1) */
           path[depth] = de1[o1].name_offset;
           repo_git_delete(depth + 1, path);
    }
    for(; o2 < dir2->size; o2++) {
           /* add(o2) */
           path[depth] = de2[o2].name_offset;
           repo_git_add(depth + 1, path, &de2[o2]);
    }
}

void
repo_diff(uint32_t r1, uint32_t r2) {
    uint32_t path_stack[1000];
    repo_diff_r(0,
        path_stack,
        repo_commit_root_dir(repo_commit_by_revision_id(r1)),
        repo_commit_root_dir(repo_commit_by_revision_id(r2)));
}
