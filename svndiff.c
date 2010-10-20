/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "compat-util.h"
#include "sliding_window.h"
#include "line_buffer.h"

/*
 * svndiff0 applier
 *
 * See http://svn.apache.org/repos/asf/subversion/trunk/notes/svndiff.
 *
 * svndiff0 ::= 'SVN\0' window window*;
 * window ::= int int int int int instructions inline_data;
 * instructions ::= instruction*;
 * instruction ::= view_selector int int
 *   | copyfrom_data int
 *   | packed_view_selector int
 *   | packed_copyfrom_data
 *   ;
 * view_selector ::= copyfrom_source
 *   | copyfrom_target
 *   ;
 * copyfrom_source ::= # binary 00 000000;
 * copyfrom_target ::= # binary 01 000000;
 * copyfrom_data ::= # binary 10 000000;
 * packed_view_selector ::= # view_selector OR-ed with 6 bit value;
 * packed_copyfrom_data ::= # copyfrom_data OR-ed with 6 bit value;
 * int ::= highdigit* lowdigit;
 * highdigit ::= # binary 1000 0000 OR-ed with 7 bit value;
 * lowdigit ::= # 7 bit value;
 */

#define INSN_MASK	0xc0
#define INSN_COPYFROM_SOURCE	0x00
#define INSN_COPYFROM_TARGET	0x40
#define INSN_COPYFROM_DATA	0x80
#define OPERAND_MASK	0x3f

#define VLI_CONTINUE	0x80
#define VLI_DIGIT_MASK	0x7f
#define VLI_BITS_PER_DIGIT 7

struct window {
	struct view *in;
	struct strbuf out;
	struct strbuf instructions;
	struct strbuf data;
};

static int write_strbuf(struct strbuf *sb, FILE *out)
{
	if (fwrite(sb->buf, 1, sb->len, out) == sb->len)	/* Success. */
		return 0;
	return error("Cannot write: %s\n", strerror(errno));
}

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

static int copyfrom_source(struct window *ctx, const char **instructions,
			   size_t nbytes, const char *insns_end)
{
	size_t offset;
	if (parse_int(instructions, &offset, insns_end))
		return -1;
	if (unsigned_add_overflows(offset, nbytes) ||
	    offset + nbytes > ctx->in->buf.len)
		return error("Invalid delta: copies source data outside view.");
	strbuf_add(&ctx->out, ctx->in->buf.buf + offset, nbytes);
	return 0;
}

static int copyfrom_target(struct window *ctx, const char **instructions,
			   size_t nbytes, const char *insns_end)
{
	size_t offset;
	if (parse_int(instructions, &offset, insns_end))
		return -1;
	if (offset >= ctx->out.len)
		return error("Invalid delta: copies from the future.");
	while (nbytes) {
		strbuf_addch(&ctx->out, ctx->out.buf[offset++]);
		nbytes--;
	}
	return 0;
}

static int copyfrom_data(struct window *ctx, size_t *data_pos, size_t nbytes)
{
	const size_t pos = *data_pos;
	if (unsigned_add_overflows(pos, nbytes) ||
	    pos + nbytes > ctx->data.len)
		return error("Invalid delta: copies unavailable inline data.");
	strbuf_add(&ctx->out, ctx->data.buf + pos, nbytes);
	*data_pos += nbytes;
	return 0;
}

static int parse_first_operand(const char **buf, size_t *out, const char *end)
{
	size_t result = (unsigned char) *(*buf)++ & OPERAND_MASK;
	if (result) {
		*out = result;
		return 0;
	}
	return parse_int(buf, out, end);
}

static int step(struct window *ctx, const char **instructions, size_t *data_pos)
{
	unsigned int instruction;
	const char *insns_end = ctx->instructions.buf + ctx->instructions.len;
	size_t nbytes;
	assert(ctx);
	assert(instructions && *instructions);
	assert(data_pos);

	instruction = (unsigned char) **instructions;
	if (parse_first_operand(instructions, &nbytes, insns_end))
		return -1;
	switch (instruction & INSN_MASK) {
	case INSN_COPYFROM_SOURCE:
		return copyfrom_source(ctx, instructions, nbytes, insns_end);
	case INSN_COPYFROM_TARGET:
		return copyfrom_target(ctx, instructions, nbytes, insns_end);
	case INSN_COPYFROM_DATA:
		return copyfrom_data(ctx, data_pos, nbytes);
	default:
		return error("Invalid instruction %x", instruction);
	}
}

static int apply_window_in_core(struct window *ctx)
{
	const char *insn = ctx->instructions.buf;
	size_t data_pos = 0;

	/*
	 * Populate ctx->out.buf using data from the source, target,
	 * and inline data views.
	 */
	while (insn != ctx->instructions.buf + ctx->instructions.len)
		if (step(ctx, &insn, &data_pos))
			return -1;
	if (data_pos != ctx->data.len)
		return error("Invalid delta: does not copy all new data");
	return 0;
}

static int apply_one_window(struct line_buffer *delta, off_t *delta_len,
			    struct view *preimage, FILE *out)
{
	struct window ctx = {preimage, STRBUF_INIT, STRBUF_INIT, STRBUF_INIT};
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
	strbuf_grow(&ctx.out, out_len);
	if (apply_window_in_core(&ctx) || write_strbuf(&ctx.out, out)) {
		rv = -1;
		goto done;
	}
	if (ctx.out.len != out_len) {
		rv = error("Invalid delta: incorrect postimage length");
		goto done;
	}
 done:
	strbuf_release(&ctx.out);
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
		    apply_one_window(delta, &delta_len,
				     &preimage_view, postimage))
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
