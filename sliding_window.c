/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "git-compat-util.h"
#include "sliding_window.h"
#include "line_buffer.h"
#include "strbuf.h"

static void strbuf_remove_from_left(struct strbuf *sb, size_t nbytes)
{
	assert(nbytes <= sb->len);
	memmove(sb->buf, sb->buf + nbytes, sb->len - nbytes);
	strbuf_setlen(sb, sb->len - nbytes);
}

static int check_overflow(off_t a, size_t b)
{
	if ((off_t) b < 0)
		return error("Unrepresentable length: "
				"%"PRIu64" > OFF_MAX", (uint64_t) b);
	if (signed_add_overflows(a, (off_t) b))
		return error("Unrepresentable offset: "
				"%"PRIu64" + %"PRIu64" > OFF_MAX",
				(uint64_t) a, (uint64_t) b);
	return 0;
}

int move_window(struct view *view, off_t off, size_t len)
{
	off_t file_offset;
	assert(view && view->file);
	assert(!check_overflow(view->off, view->buf.len));

	if (check_overflow(off, len))
		return -1;
	if (off < view->off || off + len < view->off + view->buf.len)
		return error("Invalid delta: window slides left");

	file_offset = view->off + view->buf.len;
	if (off < file_offset)
		/* Move the overlapping region into place. */
		strbuf_remove_from_left(&view->buf, off - view->off);
	else
		strbuf_setlen(&view->buf, 0);
	if (off > file_offset) {
		/* Seek ahead to skip the gap. */
		const off_t gap = off - file_offset;
		const off_t nread = buffer_skip_bytes(view->file, gap);
		if (nread != gap) {
			if (!buffer_ferror(view->file))
				return error("Preimage ends early");
			return error("Cannot seek forward in input: %s",
				     strerror(errno));
		}
		file_offset += gap;
	}
	buffer_read_binary(&view->buf, len - view->buf.len, view->file);
	if (view->buf.len != len) {
		if (!buffer_ferror(view->file))
			return error("Preimage ends early");
		return error("Cannot read preimage: %s", strerror(errno));
	}
	view->off = off;
	return 0;
}
