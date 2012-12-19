/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "compat-util.h"
#include "git2.h"
#include "strbuf.h"
#include "quote.h"
#include "fast_export.h"
#include "repo_tree.h"
#include "strbuf.h"
#include "svndiff.h"
#include "sliding_window.h"
#include "line_buffer.h"

#define MAX_GITSVN_LINE_LEN 4096

static uint32_t first_commit_done;
static struct line_buffer postimage = LINE_BUFFER_INIT;
static git_odb *odb;
static git_repository *repo;
static git_index *fe_index;
static struct strbuf marks = STRBUF_INIT;
static struct strbuf paths = STRBUF_INIT;

static void set_mark(int mark, git_oid *oid)
{
	assert(mark > 0 && oid);
	strbuf_grow(&marks, mark * sizeof(git_oid));
	git_oid_cpy((git_oid*)(marks.buf) + mark - 1, oid);
}

static void get_mark(int mark, git_oid *oid)
{
	assert(mark > 0 && oid);
	if (marks.alloc > mark * sizeof(git_oid))
		git_oid_cpy(oid, (git_oid*)(marks.buf) + mark - 1);
}

/* NEEDSWORK: move to fast_export_init() */
static int init_postimage(void)
{
	static int postimage_initialized;
	if (postimage_initialized)
		return 0;
	postimage_initialized = 1;
	return buffer_tmpfile_init(&postimage);
}

void fast_export_init(git_repository *repo_in)
{
	first_commit_done = 0;
	repo = repo_in;
	git_repository_odb(&odb, repo);
	git_index_new(&fe_index);
}

void fast_export_deinit(void)
{
	git_odb_free(odb);
	odb = NULL;
	strbuf_release(&marks);
	strbuf_release(&paths);
}

void fast_export_delete(const char *path)
{
	const git_index_entry *entry = git_index_get_bypath(fe_index, path, 0);
	if (!entry) {
		size_t n = 0;
		size_t path_len = strlen(path);
		size_t npaths = 0;
		while ((entry = git_index_get_byindex(fe_index, n++)))
			if (!strncmp(path, entry->path, path_len) && entry->path[path_len] == '/') {
				strbuf_grow(&paths, ++npaths * sizeof(char*));
				((const char**)(paths.buf))[npaths - 1] = entry->path;
			}
		while (npaths--)
			git_index_remove(fe_index, ((const char**)(paths.buf))[npaths], 0);
		return;
	}
	git_index_remove(fe_index, path, 0);
}

static void fast_export_truncate(git_oid *oid)
{
	git_blob_create_frombuffer(oid, repo, "", 0);
}

void fast_export_modify(const char *path, uint32_t mode, const git_oid *dataref)
{
	git_index_entry entry;
	memset(&entry, 0, sizeof(entry));
	/* Mode must be 100644, 100755, 120000, or 160000. */
	if (!dataref)
		fast_export_truncate(&entry.oid);
	else if (mode == REPO_MODE_DIR) {
		git_index *index;
		git_tree *tree;
		size_t n = 0;
		const git_index_entry *subentry;
		fast_export_delete(path);
		git_index_new(&index);
		git_tree_lookup(&tree, repo, dataref);
		git_index_read_tree(index, tree);
		git_tree_free(tree);
		while ((subentry = git_index_get_byindex(index, n++))) {
			entry = *subentry;
			asprintf(&entry.path, "%s/%s", path, subentry->path);
			git_index_add(fe_index, &entry);
			free(entry.path);
		}
		git_index_free(index);
		return;
	} else
		git_oid_cpy(&entry.oid, dataref);
	entry.mode = mode;
	entry.path = (char *)path;
	git_index_add(fe_index, &entry);
}

static struct {
	int mark;
	char *ref;
	git_signature author;
	git_signature committer;
	char *message;
	int has_parent;
	git_commit *parent;
} commit;
void fast_export_begin_commit(uint32_t revision, const char *author,
			const struct strbuf *log,
			const char *uuid, const char *url,
			unsigned long timestamp)
{
	static const struct strbuf empty = STRBUF_INIT;
	char *gitsvnline;
	git_oid parent;
	if (!log)
		log = &empty;
	if (*uuid && *url) {
		asprintf(&gitsvnline,
				"\n\ngit-svn-id: %s@%"PRIu32" %s\n",
				 url, revision, uuid);
	} else {
		gitsvnline = NULL;
	}
	commit.ref = "refs/heads/master";
	commit.mark = revision;
	asprintf(&commit.committer.name, "%s",
		 *author ? author : "nobody");
	asprintf(&commit.committer.email, "%s@%s",
		 *author ? author : "nobody",
		 *uuid ? uuid : "local");
	commit.committer.when.time = timestamp;
	commit.committer.when.offset = 0;
	commit.author = commit.committer;
	asprintf(&commit.message, "%*s%s",
		 (int)log->len, log->buf,
		 gitsvnline ? gitsvnline : "");
	commit.has_parent = revision > 1;
	if (commit.has_parent) {
		get_mark(revision - 1, &parent);
		git_commit_lookup(&commit.parent, repo, &parent);
	}
	if (!first_commit_done) {
		first_commit_done = 1;
	}
}

void fast_export_end_commit(uint32_t revision)
{
	char ohex[40];
	git_oid oid;
	git_tree *tree;
	git_index_write_tree_to(&oid, fe_index, repo);
	git_tree_lookup(&tree, repo, &oid);
	git_commit_create(
		&oid,
		repo,
		commit.ref,
		&commit.author,
		&commit.committer,
		NULL,
		commit.message,
		tree,
		commit.has_parent,
		(const git_commit**)&commit.parent);
	set_mark(commit.mark, &oid);
	git_oid_fmt(ohex, &oid);
	printf("progress Imported commit %"PRIu32": %*s\n\n", revision, 40, ohex);
}

static void die_short_read(struct line_buffer *input)
{
	if (buffer_ferror(input))
		die_errno("error reading dump file");
	die("invalid dump: unexpected end of file");
}

static void check_preimage_overflow(off_t a, off_t b)
{
	if (signed_add_overflows(a, b))
		die("blob too large for current definition of off_t");
}

static long apply_delta(off_t len, struct line_buffer *input,
			const git_oid *old_data, uint32_t old_mode)
{
	long ret;
	struct line_buffer preimage_buffer = LINE_BUFFER_INIT;
	struct sliding_view preimage = SLIDING_VIEW_INIT(&preimage_buffer, 0);
	FILE *out;
	git_blob *old_blob = NULL;

	if (init_postimage() || !(out = buffer_tmpfile_rewind(&postimage)))
		die("cannot open temporary file for blob retrieval");
	if (old_data) {
		git_blob_lookup(&old_blob, repo, old_data);
		preimage_buffer.infile =
			fmemopen(git_blob_rawcontent(old_blob),
				 git_blob_rawsize(old_blob),
				 "r");
		preimage.max_off = git_blob_rawsize(old_blob);
		check_preimage_overflow(preimage.max_off, 1);
	}
	if (old_mode == REPO_MODE_LNK) {
		strbuf_addstr(&preimage.buf, "link ");
		check_preimage_overflow(preimage.max_off, strlen("link "));
		preimage.max_off += strlen("link ");
		check_preimage_overflow(preimage.max_off, 1);
	}
	if (svndiff0_apply(input, len, &preimage, out))
		die("cannot apply delta");
	if (old_data) {
		git_blob_free(old_blob);
	}
	ret = buffer_tmpfile_prepare_to_read(&postimage);
	if (ret < 0)
		die("cannot read temporary file for blob retrieval");
	strbuf_release(&preimage.buf);
	return ret;
}

struct line_buffer_limit {
	struct line_buffer *input;
	off_t remaining;
};

static int line_buffer_cb(char *content, size_t max_length, void *payload)
{
	struct line_buffer_limit *stream = payload;
	struct strbuf buffer = { max_length--, 0, content };
	int ret;
	if (!stream->remaining)
		return 0;
	if (stream->remaining < (off_t) max_length)
		max_length = stream->remaining;
	ret = buffer_read_binary(stream->input, &buffer, max_length);
	if (ret > 0)
		stream->remaining -= ret;
	return ret;
}

void fast_export_data(git_oid *oid, uint32_t mode, off_t len, struct line_buffer *input)
{
	struct line_buffer_limit payload = { input, len };
	assert(len >= 0);
	if (mode == REPO_MODE_LNK) {
		/* svn symlink blobs start with "link " */
		if (len < 5)
			die("invalid dump: symlink too short for \"link\" prefix");
		len -= 5;
		if (buffer_skip_bytes(input, 5) != 5)
			die_short_read(input);
	}
	git_blob_create_fromchunks(oid, repo, NULL, &line_buffer_cb, &payload);
}

int fast_export_ls_rev(uint32_t rev, const char *path,
				uint32_t *mode, git_oid *dataref_out)
{
	git_oid oid;
	git_commit *commit;
	git_tree *root;
	git_tree_entry *entry;
	get_mark(rev, &oid);
	git_commit_lookup(&commit, repo, &oid);
	git_commit_tree(&root, commit);
	if (git_tree_entry_bypath(&entry, root, path) < 0) {
		git_tree_free(root);
		git_commit_free(commit);
		errno = ENOENT;
		return -1;
	}
	*mode = git_tree_entry_filemode(entry);
	git_oid_cpy(dataref_out, git_tree_entry_id(entry));
	git_tree_entry_free(entry);
	git_tree_free(root);
	git_commit_free(commit);
	return 0;
}

int fast_export_ls(const char *path, uint32_t *mode, git_oid *dataref_out)
{
	const git_index_entry *entry = git_index_get_bypath(fe_index, path, 0);
	if (!entry) {
		git_oid oid;
		git_tree *root;
		git_tree_entry *entry;
		git_index_write_tree_to(&oid, fe_index, repo);
		git_tree_lookup(&root, repo, &oid);
		if (git_tree_entry_bypath(&entry, root, path) < 0) {
			git_tree_free(root);
			errno = ENOENT;
			return -1;
		}
		*mode = git_tree_entry_filemode(entry);
		git_oid_cpy(dataref_out, git_tree_entry_id(entry));
		git_tree_entry_free(entry);
		git_tree_free(root);
		return 0;
	}
	*mode = entry->mode;
	git_oid_cpy(dataref_out, &entry->oid);
	return 0;
}

void fast_export_blob_delta(git_oid *oid, uint32_t mode,
				uint32_t old_mode, const git_oid *old_data,
				off_t len, struct line_buffer *input)
{
	struct line_buffer_limit payload = { &postimage, 0 };
	long postimage_len;

	assert(len >= 0);
	postimage_len = apply_delta(len, input, old_data, old_mode);
	if (mode == REPO_MODE_LNK) {
		buffer_skip_bytes(&postimage, strlen("link "));
		postimage_len -= strlen("link ");
	}
	payload.remaining = postimage_len;
	git_blob_create_fromchunks(oid, repo, NULL, &line_buffer_cb, &payload);
}
