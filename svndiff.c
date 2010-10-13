/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "git-compat-util.h"
#include "sliding_window.h"
#include "line_buffer.h"

/*
 * svndiff0 applier
 *
 * See http://svn.apache.org/repos/asf/subversion/trunk/notes/svndiff.
 *
 * svndiff0 ::= 'SVN\0' window window*;
 * window ::= int int int int int instructions inline_data;
 * int ::= highdigit* lowdigit;
 * highdigit ::= # binary 1000 0000 OR-ed with 7 bit value;
 * lowdigit ::= # 7 bit value;
 */

#define VLI_CONTINUE	0x80
#define VLI_DIGIT_MASK	0x7f
#define VLI_BITS_PER_DIGIT 7

struct window {
	struct strbuf instructions;
	struct strbuf data;
};

static int read_magic(struct line_buffer *in, off_t *len)
{
	static const char magic[] = {'S', 'V', 'N', '\0'};
	struct strbuf sb = STRBUF_INIT;
	if (*len < sizeof(magic))
		return error("Invalid delta: no file type header");
	buffer_read_binary(&sb, sizeof(magic), in);
	if (sb.len != sizeof(magic))
		return error("Invalid delta: no file type header");
	if (memcmp(sb.buf, magic, sizeof(magic)))
		return error("Unrecognized file type %.*s",
			     (int) sizeof(magic), sb.buf);
	*len -= sizeof(magic);
	strbuf_release(&sb);
	return 0;
}

static int read_int(struct line_buffer *in, uintmax_t *result, off_t *len)
{
	off_t sz = *len;
	uintmax_t rv = 0;
	while (sz) {
		int ch = buffer_read_char(in);
		if (ch == EOF)
			return error("Delta ends early (%"PRIu64" bytes remaining)",
				     (uint64_t) sz);
		sz--;
		rv <<= VLI_BITS_PER_DIGIT;
		rv += (ch & VLI_DIGIT_MASK);
		if (!(ch & VLI_CONTINUE)) {
			*result = rv;
			*len = sz;
			return 0;
		}
	}
	return error("Invalid delta: incomplete integer %"PRIuMAX, rv);
}

static int parse_int(const char **buf, size_t *result, const char *end)
{
	const char *pos;
	size_t rv = 0;
	for (pos = *buf; pos != end; pos++) {
		unsigned char ch = *pos;
		rv <<= VLI_BITS_PER_DIGIT;
		rv += (ch & VLI_DIGIT_MASK);
		if (!(ch & VLI_CONTINUE)) {
			*result = rv;
			*buf = pos + 1;
			return 0;
		}
	}
	return error("Invalid instruction: incomplete integer %"PRIu64,
		     (uint64_t) rv);
}

static int read_offset(struct line_buffer *in, off_t *result, off_t *len)
{
	uintmax_t val;
	if (read_int(in, &val, len))
		return -1;
	if (val > maximum_signed_value_of_type(off_t))
		return error("Unrepresentable offset: %"PRIuMAX, val);
	*result = val;
	return 0;
}

static int read_length(struct line_buffer *in, size_t *result, off_t *len)
{
	uintmax_t val;
	if (read_int(in, &val, len))
		return -1;
	if (val > SIZE_MAX)
		return error("Unrepresentable length: %"PRIuMAX, val);
	*result = val;
	return 0;
}

static int read_chunk(struct line_buffer *delta, off_t *delta_len,
		      struct strbuf *buf, size_t len)
{
	if (len > maximum_signed_value_of_type(off_t) ||
	    (off_t) len > *delta_len)
		return -1;
	strbuf_reset(buf);
	buffer_read_binary(buf, len, delta);
	*delta_len -= buf->len;
	return 0;
}

static int apply_one_window(struct line_buffer *delta, off_t *delta_len)
{
	struct window ctx = {STRBUF_INIT, STRBUF_INIT};
	size_t out_len;
	size_t instructions_len;
	size_t data_len;
	int rv = 0;
	assert(delta_len);

	/* "source view" offset and length already handled; */
	if (read_length(delta, &out_len, delta_len) ||
	    read_length(delta, &instructions_len, delta_len) ||
	    read_length(delta, &data_len, delta_len))
		return -1;
	if (read_chunk(delta, delta_len, &ctx.instructions, instructions_len))
		return error("Invalid delta: incomplete instructions section");
	if (buffer_ferror(delta)) {
		rv = error("Cannot read delta: %s", strerror(errno));
		goto done;
	}
	if (read_chunk(delta, delta_len, &ctx.data, data_len)) {
		rv = error("Invalid delta: incomplete data section");
		goto done;
	}
	if (buffer_ferror(delta)) {
		rv = error("Cannot read delta: %s", strerror(errno));
		goto done;
	}
	if (instructions_len > 0)
		return error("What do you think I am?  A delta applier?");
 done:
	strbuf_release(&ctx.data);
	strbuf_release(&ctx.instructions);
	return rv;
}

int svndiff0_apply(struct line_buffer *delta, off_t delta_len,
		   struct line_buffer *preimage, FILE *postimage)
{
	struct view preimage_view = {preimage, 0, STRBUF_INIT};
	assert(delta && preimage && postimage);

	if (read_magic(delta, &delta_len))
		goto fail;
	while (delta_len > 0) {	/* For each window: */
		off_t pre_off = pre_off;
		size_t pre_len;
		if (read_offset(delta, &pre_off, &delta_len) ||
		    read_length(delta, &pre_len, &delta_len) ||
		    move_window(&preimage_view, pre_off, pre_len) ||
		    apply_one_window(delta, &delta_len))
			goto fail;
		if (delta_len && buffer_at_eof(delta)) {
			error("Delta ends early! (%"PRIu64" bytes remaining)",
			      (uint64_t) delta_len);
			goto fail;
		}
	}
	strbuf_release(&preimage_view.buf);
	return 0;
 fail:
	strbuf_release(&preimage_view.buf);
	return -1;
}
