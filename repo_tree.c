#include <string.h>

#include "string_pool.h"
#include "repo_tree.h"
#include "obj_pool.h"
#include "fast_export.h"

struct repo_dirent {
	uint32_t name_offset;
	uint32_t mode;
	uint32_t content_offset;
};

struct repo_dir {
	uint32_t size;
	uint32_t first_offset;
};

struct repo_commit {
	uint32_t mark;
	uint32_t root_dir_offset;
};

/* Generate memory pools for commit, dir and dirent */
obj_pool_gen(commit, struct repo_commit, 4096);
obj_pool_gen(dir, struct repo_dir, 4096);
obj_pool_gen(dirent, struct repo_dirent, 4096);

static uint32_t num_dirs_saved = 0;
static uint32_t num_dirents_saved;
static uint32_t active_commit = -1;

static struct repo_dir *repo_commit_root_dir(struct repo_commit *commit)
{
	return dir_pointer(commit->root_dir_offset);
}

static struct repo_dirent *repo_first_dirent(struct repo_dir *dir)
{
	return dirent_pointer(dir->first_offset);
}

static int repo_dirent_name_cmp(const void *a, const void *b)
{
	const struct repo_dirent *dirent1 = a, *dirent2 = b;
	uint32_t a_offset = dirent1->name_offset;
	uint32_t b_offset = dirent2->name_offset;
	return (a_offset > b_offset) - (a_offset < b_offset);
}

static struct repo_dirent *repo_dirent_by_name(struct repo_dir *dir,
                                          uint32_t name_offset)
{
	struct repo_dirent key;
	if (dir == NULL || dir->size == 0)
		return NULL;
	key.name_offset = name_offset;
	return bsearch(&key, repo_first_dirent(dir), dir->size,
				   sizeof(struct repo_dirent), repo_dirent_name_cmp);
}

static int repo_dirent_is_dir(struct repo_dirent *dirent)
{
	return dirent != NULL && dirent->mode == REPO_MODE_DIR;
}

static struct repo_dir *repo_dir_from_dirent(struct repo_dirent *dirent)
{
	if (!repo_dirent_is_dir(dirent))
		return NULL;
	return dir_pointer(dirent->content_offset);
}

static uint32_t dir_with_dirents_alloc(uint32_t size)
{
	uint32_t offset = dir_alloc(1);
	dir_pointer(offset)->size = size;
	dir_pointer(offset)->first_offset = dirent_alloc(size);
	return offset;
}

static struct repo_dir *repo_clone_dir(struct repo_dir *orig_dir, uint32_t padding)
{
	uint32_t orig_o, new_o, dirent_o;
	orig_o = dir_offset(orig_dir);
	if (orig_o < num_dirs_saved) {
		new_o = dir_with_dirents_alloc(orig_dir->size + padding);
		orig_dir = dir_pointer(orig_o);
		dirent_o = dir_pointer(new_o)->first_offset;
	} else {
		if (padding == 0)
			return orig_dir;
		new_o = orig_o;
		dirent_o = dirent_alloc(orig_dir->size + padding);
	}
	memcpy(dirent_pointer(dirent_o), repo_first_dirent(orig_dir),
		   orig_dir->size * sizeof(struct repo_dirent));
	dir_pointer(new_o)->size = orig_dir->size + padding;
	dir_pointer(new_o)->first_offset = dirent_o;
	return dir_pointer(new_o);
}

static struct repo_dirent *repo_read_dirent(uint32_t revision, uint32_t *path)
{
	uint32_t name = 0;
	struct repo_dir *dir = NULL;
	struct repo_dirent *dirent = NULL;
	dir = repo_commit_root_dir(commit_pointer(revision));
	while (~(name = *path++)) {
		dirent = repo_dirent_by_name(dir, name);
		if (dirent == NULL) {
			return NULL;
		} else if (repo_dirent_is_dir(dirent)) {
			dir = repo_dir_from_dirent(dirent);
		} else {
			break;
		}
	}
	return dirent;
}

static void
repo_write_dirent(uint32_t *path, uint32_t mode, uint32_t content_offset,
                  uint32_t del)
{
	uint32_t name, revision, dirent_o = ~0, dir_o = ~0, parent_dir_o = ~0;
	struct repo_dir *dir;
	struct repo_dirent *dirent = NULL;
	revision = active_commit;
	dir = repo_commit_root_dir(commit_pointer(revision));
	dir = repo_clone_dir(dir, 0);
	commit_pointer(revision)->root_dir_offset = dir_offset(dir);
	while (~(name = *path++)) {
		parent_dir_o = dir_offset(dir);
		dirent = repo_dirent_by_name(dir, name);
		if (dirent == NULL) {
			dir = repo_clone_dir(dir, 1);
			dirent = &repo_first_dirent(dir)[dir->size - 1];
			dirent->name_offset = name;
			dirent->mode = REPO_MODE_DIR;
			qsort(repo_first_dirent(dir), dir->size,
				  sizeof(struct repo_dirent), repo_dirent_name_cmp);
			dirent = repo_dirent_by_name(dir, name);
			dir_o = dir_with_dirents_alloc(0);
			dirent->content_offset = dir_o;
			dir = dir_pointer(dir_o);
		} else if ((dir = repo_dir_from_dirent(dirent))) {
			dirent_o = dirent_offset(dirent);
			dir = repo_clone_dir(dir, 0);
			if (dirent_o != ~0)
				dirent_pointer(dirent_o)->content_offset = dir_offset(dir);
		} else {
			dirent->mode = REPO_MODE_DIR;
			dirent_o = dirent_offset(dirent);
			dir_o = dir_with_dirents_alloc(0);
			dirent = dirent_pointer(dirent_o);
			dir = dir_pointer(dir_o);
			dirent->content_offset = dir_o;
		}
	}
	if (dirent) {
		dirent->mode = mode;
		dirent->content_offset = content_offset;
		if (del && ~parent_dir_o) {
			dirent->name_offset = ~0;
			dir = dir_pointer(parent_dir_o);
			qsort(repo_first_dirent(dir), dir->size,
				  sizeof(struct repo_dirent), repo_dirent_name_cmp);
			dir->size--;
		}
	}
}

uint32_t repo_copy(uint32_t revision, uint32_t *src, uint32_t *dst)
{
	uint32_t mode = 0, content_offset = 0;
	struct repo_dirent *src_dirent;
	src_dirent = repo_read_dirent(revision, src);
	if (src_dirent != NULL) {
		mode = src_dirent->mode;
		content_offset = src_dirent->content_offset;
		repo_write_dirent(dst, mode, content_offset, 0);
	}
	return mode;
}

void repo_add(uint32_t *path, uint32_t mode, uint32_t blob_mark)
{
	repo_write_dirent(path, mode, blob_mark, 0);
}

uint32_t repo_replace(uint32_t *path, uint32_t blob_mark)
{
	uint32_t mode = 0;
	struct repo_dirent *src_dirent;
	src_dirent = repo_read_dirent(active_commit, path);
	if (src_dirent != NULL) {
		mode = src_dirent->mode;
		repo_write_dirent(path, mode, blob_mark, 0);
	}
	return mode;
}

void repo_modify(uint32_t *path, uint32_t mode, uint32_t blob_mark)
{
	struct repo_dirent *src_dirent;
	src_dirent = repo_read_dirent(active_commit, path);
	if (src_dirent != NULL && blob_mark == 0) {
		blob_mark = src_dirent->content_offset;
	}
	repo_write_dirent(path, mode, blob_mark, 0);
}

void repo_delete(uint32_t *path)
{
	repo_write_dirent(path, 0, 0, 1);
}

static void repo_git_add_r(uint32_t depth, uint32_t *path, struct repo_dir *dir);

static void repo_git_add(uint32_t depth, uint32_t *path, struct repo_dirent *dirent)
{
	if (repo_dirent_is_dir(dirent)) {
		repo_git_add_r(depth, path, repo_dir_from_dirent(dirent));
	} else {
		fast_export_modify(depth, path, dirent->mode, dirent->content_offset);
	}
}

static void repo_git_add_r(uint32_t depth, uint32_t *path, struct repo_dir *dir)
{
	uint32_t o;
	struct repo_dirent *de;
	de = repo_first_dirent(dir);
	for (o = 0; o < dir->size; o++) {
		path[depth] = de[o].name_offset;
		repo_git_add(depth + 1, path, &de[o]);
	}
}

static void repo_diff_r(uint32_t depth, uint32_t *path, struct repo_dir *dir1,
			struct repo_dir *dir2)
{
	struct repo_dirent *de1, *de2, *max_de1, *max_de2;
	de1 = repo_first_dirent(dir1);
	de2 = repo_first_dirent(dir2);
	max_de1 = &de1[dir1->size];
	max_de2 = &de2[dir2->size];

	while (de1 < max_de1 && de2 < max_de2) {
		if (de1->name_offset < de2->name_offset) {
			path[depth] = (de1++)->name_offset;
			fast_export_delete(depth + 1, path);
		} else if (de1->name_offset > de2->name_offset) {
			path[depth] = de2->name_offset;
			repo_git_add(depth + 1, path, de2++);
		} else {
			path[depth] = de1->name_offset;
			if (de1->mode != de2->mode ||
				de1->content_offset != de2->content_offset) {
				if (repo_dirent_is_dir(de1) && repo_dirent_is_dir(de2)) {
					repo_diff_r(depth + 1, path,
								repo_dir_from_dirent(de1),
								repo_dir_from_dirent(de2));
				} else {
					if (repo_dirent_is_dir(de1) != repo_dirent_is_dir(de2)) {
						fast_export_delete(depth + 1, path);
					}
					repo_git_add(depth + 1, path, de2);
				}
			}
			de1++;
			de2++;
		}
	}
	while (de1 < max_de1) {
		path[depth] = (de1++)->name_offset;
		fast_export_delete(depth + 1, path);
	}
	while (de2 < max_de2) {
		path[depth] = de2->name_offset;
		repo_git_add(depth + 1, path, de2++);
	}
}

static uint32_t path_stack[REPO_MAX_PATH_DEPTH];

void repo_diff(uint32_t r1, uint32_t r2)
{
	repo_diff_r(0,
		    path_stack,
		    repo_commit_root_dir(commit_pointer(r1)),
		    repo_commit_root_dir(commit_pointer(r2)));
}

void repo_commit(uint32_t revision, uint32_t author, char *log, uint32_t uuid,
                 uint32_t url, time_t timestamp)
{
	if (revision == 0) {
		active_commit = commit_alloc(1);
		commit_pointer(active_commit)->root_dir_offset =
			dir_with_dirents_alloc(0);
	} else {
		fast_export_commit(revision, author, log, uuid, url, timestamp);
	}
	num_dirs_saved = dir_pool.size;
	num_dirents_saved = dirent_pool.size;
	active_commit = commit_alloc(1);
	commit_pointer(active_commit)->root_dir_offset =
		commit_pointer(active_commit - 1)->root_dir_offset;
}

void repo_reset(void)
{
	pool_reset();
	commit_reset();
	dir_reset();
	dirent_reset();
}
