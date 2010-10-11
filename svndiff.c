/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "git-compat-util.h"
#include "line_buffer.h"

/*
 * svndiff0 applier
 *
 * See http://svn.apache.org/repos/asf/subversion/trunk/notes/svndiff.
 *
 * svndiff0 ::= 'SVN\0' window window*;
 * int ::= highdigit* lowdigit;
 * highdigit ::= # binary 1000 0000 OR-ed with 7 bit value;
 * lowdigit ::= # 7 bit value;
 */

#define VLI_CONTINUE	0x80
#define VLI_DIGIT_MASK	0x7f
#define VLI_BITS_PER_DIGIT 7

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
