/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "compat-util.h"
#include "fast_export.h"
#include "line_buffer.h"
#include "repo_tree.h"
#include "string_pool.h"
#include "svndiff.h"

#define MAX_GITSVN_LINE_LEN 4096
#define REPORT_FILENO 3

#define SHA1_HEX_LENGTH 40

static uint32_t first_commit_done;
static struct line_buffer preimage = LINE_BUFFER_INIT;
static struct line_buffer postimage = LINE_BUFFER_INIT;
static struct line_buffer backchannel = LINE_BUFFER_INIT;

void fast_export_delete(uint32_t depth, uint32_t *path)
{
	putchar('D');
	putchar(' ');
	pool_print_seq(depth, path, '/', stdout);
	putchar('\n');
}

void fast_export_modify(uint32_t depth, uint32_t *path, uint32_t mode,
			uint32_t mark)
{
	/* Mode must be 100644, 100755, 120000, or 160000. */
	printf("M %06"PRIo32" :%"PRIu32" ", mode, mark);
	pool_print_seq(depth, path, '/', stdout);
	putchar('\n');
}

static char gitsvnline[MAX_GITSVN_LINE_LEN];
void fast_export_commit(uint32_t revision, uint32_t author, char *log,
			uint32_t uuid, uint32_t url,
			unsigned long timestamp)
{
	if (!log)
		log = "";
	if (~uuid && ~url) {
		snprintf(gitsvnline, MAX_GITSVN_LINE_LEN,
				"\n\ngit-svn-id: %s@%"PRIu32" %s\n",
				 pool_fetch(url), revision, pool_fetch(uuid));
	} else {
		*gitsvnline = '\0';
	}
	printf("commit refs/heads/master\n");
	printf("committer %s <%s@%s> %ld +0000\n",
		   ~author ? pool_fetch(author) : "nobody",
		   ~author ? pool_fetch(author) : "nobody",
		   ~uuid ? pool_fetch(uuid) : "local", timestamp);
	printf("data %"PRIu32"\n%s%s\n",
		   (uint32_t) (strlen(log) + strlen(gitsvnline)),
		   log, gitsvnline);
	if (!first_commit_done) {
		if (revision > 1)
			printf("from refs/heads/master^0\n");
		first_commit_done = 1;
	}
	repo_diff(revision - 1, revision);
	fputc('\n', stdout);

	printf("progress Imported commit %"PRIu32".\n\n", revision);
}

static int fast_export_save_blob(FILE *out)
{
	size_t len;
	char *header;
	char *end;
	char *tail;

	if (!backchannel.infile)
		backchannel.infile = fdopen(REPORT_FILENO, "r");
	if (!backchannel.infile)
		return error("Could not open backchannel fd: %d", REPORT_FILENO);
	header = buffer_read_line(&backchannel);
	if (header == NULL)
		return 1;
	end = strchr(header, '\0');
	if (end - header > 7 && !strcmp(end - 7, "missing"))
		return error("cat-blob reports missing blob: %s", header);
	if (end - header < SHA1_HEX_LENGTH)
		return error("cat-blob header too short for SHA1: %s", header);
	if (strncmp(header + SHA1_HEX_LENGTH, " blob ", 6))
		return error("cat-blob header has wrong object type: %s", header);
	len = strtoumax(header + SHA1_HEX_LENGTH + 6, &end, 10);
	if (end == header + SHA1_HEX_LENGTH + 6)
		return error("cat-blob header did not contain length: %s", header);
	if (*end)
		return error("cat-blob header contained garbage after length: %s", header);
	buffer_copy_bytes(&backchannel, out, len);
	tail = buffer_read_line(&backchannel);
	if (!tail)
		return 1;
	if (*tail)
		return error("cat-blob trailing line contained garbage: %s", tail);
	return 0;
}

void fast_export_blob(uint32_t mode, uint32_t mark, uint32_t len,
			uint32_t delta, uint32_t srcMark, uint32_t srcMode,
			struct line_buffer *input)
{
	long preimage_len = 0;

	if (delta) {
		if (!preimage.infile)
			preimage.infile = tmpfile();
		if (!preimage.infile)
			die("Unable to open temp file for blob retrieval");
		if (srcMark) {
			printf("cat-blob :%"PRIu32"\n", srcMark);
			fflush(stdout);
			if (srcMode == REPO_MODE_LNK)
				fwrite("link ", 1, 5, preimage.infile);
			if (fast_export_save_blob(preimage.infile))
				die("Failed to retrieve blob for delta application");
		}
		preimage_len = ftell(preimage.infile);
		fseek(preimage.infile, 0, SEEK_SET);
		if (!postimage.infile)
			postimage.infile = tmpfile();
		if (!postimage.infile)
			die("Unable to open temp file for blob application");
		svndiff0_apply(input, len, &preimage, postimage.infile);
		len = ftell(postimage.infile);
		fseek(postimage.infile, 0, SEEK_SET);
	}

	if (mode == REPO_MODE_LNK) {
		/* svn symlink blobs start with "link " */
		if (delta)
			buffer_skip_bytes(&postimage, 5);
		else
			buffer_skip_bytes(input, 5);
		len -= 5;
	}
	printf("blob\nmark :%"PRIu32"\ndata %"PRIu32"\n", mark, len);
	if (!delta)
		buffer_copy_bytes(input, stdout, len);
	else
		buffer_copy_bytes(&postimage, stdout, len);
	fputc('\n', stdout);

	if (preimage.infile) {
		fseek(preimage.infile, 0, SEEK_SET);
	}

	if (postimage.infile) {
		fseek(postimage.infile, 0, SEEK_SET);
	}
}
