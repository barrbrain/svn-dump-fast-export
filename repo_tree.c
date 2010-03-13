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

#include <stdlib.h>
#include <stdint.h>

#include "repo_tree.h"

static repo_dir_t* repo_dir_base;

static repo_dir_t*
repo_commit_root_dir(repo_commit_t *commit) {
    return &repo_dir_base[commit->root_dir_offset];
}

static repo_dirent_t* repo_dirent_base;

static repo_dirent_t*
repo_first_dirent(repo_dir_t* dir) {
    return &repo_dirent_base[dir->first_offset];
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
    return &repo_dir_base[dirent->content_offset];
}
