/*
 * From git 1.7.10.2
 * License: GPL-2
 *
 * Modifications (2012-05-18):
 *  - add license header.
 *  - use compat-util.h in place of cache.h.
 *  - include strbuf.h directly instead of through refs.h.
 *  - remove unneeded functions.
 */

#include "compat-util.h"
#include "quote.h"
#include "strbuf.h"

int quote_path_fully = 1;

/* 1 means: quote as octal
 * 0 means: quote as octal if (quote_path_fully)
 * -1 means: never quote
 * c: quote as "\\c"
 */
#define X8(x)   x, x, x, x, x, x, x, x
#define X16(x)  X8(x), X8(x)
static signed char const sq_lookup[256] = {
	/*           0    1    2    3    4    5    6    7 */
	/* 0x00 */   1,   1,   1,   1,   1,   1,   1, 'a',
	/* 0x08 */ 'b', 't', 'n', 'v', 'f', 'r',   1,   1,
	/* 0x10 */ X16(1),
	/* 0x20 */  -1,  -1, '"',  -1,  -1,  -1,  -1,  -1,
	/* 0x28 */ X16(-1), X16(-1), X16(-1),
	/* 0x58 */  -1,  -1,  -1,  -1,'\\',  -1,  -1,  -1,
	/* 0x60 */ X16(-1), X8(-1),
	/* 0x78 */  -1,  -1,  -1,  -1,  -1,  -1,  -1,   1,
	/* 0x80 */ /* set to 0 */
};

static inline int sq_must_quote(char c)
{
	return sq_lookup[(unsigned char)c] + quote_path_fully > 0;
}

/* returns the longest prefix not needing a quote up to maxlen if positive.
   This stops at the first \0 because it's marked as a character needing an
   escape */
static size_t next_quote_pos(const char *s, ssize_t maxlen)
{
	ssize_t len;
	if (maxlen < 0) {
		for (len = 0; !sq_must_quote(s[len]); len++);
	} else {
		for (len = 0; len < maxlen && !sq_must_quote(s[len]); len++);
	}
	return len;
}

/*
 * C-style name quoting.
 *
 * (1) if sb and fp are both NULL, inspect the input name and counts the
 *     number of bytes that are needed to hold c_style quoted version of name,
 *     counting the double quotes around it but not terminating NUL, and
 *     returns it.
 *     However, if name does not need c_style quoting, it returns 0.
 *
 * (2) if sb or fp are not NULL, it emits the c_style quoted version
 *     of name, enclosed with double quotes if asked and needed only.
 *     Return value is the same as in (1).
 */
static size_t quote_c_style_counted(const char *name, ssize_t maxlen,
                                    struct strbuf *sb, FILE *fp, int no_dq)
{
#undef EMIT
#define EMIT(c)                                 \
	do {                                        \
		if (sb) strbuf_addch(sb, (c));          \
		if (fp) fputc((c), fp);                 \
		count++;                                \
	} while (0)
#define EMITBUF(s, l)                           \
	do {                                        \
		if (sb) strbuf_add(sb, (s), (l));       \
		if (fp) fwrite((s), (l), 1, fp);        \
		count += (l);                           \
	} while (0)

	ssize_t len, count = 0;
	const char *p = name;

	for (;;) {
		int ch;

		len = next_quote_pos(p, maxlen);
		if (len == maxlen || (maxlen < 0 && !p[len]))
			break;

		if (!no_dq && p == name)
			EMIT('"');

		EMITBUF(p, len);
		EMIT('\\');
		p += len;
		ch = (unsigned char)*p++;
		if (maxlen >= 0)
			maxlen -= len + 1;
		if (sq_lookup[ch] >= ' ') {
			EMIT(sq_lookup[ch]);
		} else {
			EMIT(((ch >> 6) & 03) + '0');
			EMIT(((ch >> 3) & 07) + '0');
			EMIT(((ch >> 0) & 07) + '0');
		}
	}

	EMITBUF(p, len);
	if (p == name)   /* no ending quote needed */
		return 0;

	if (!no_dq)
		EMIT('"');
	return count;
}

size_t quote_c_style(const char *name, struct strbuf *sb, FILE *fp, int nodq)
{
	return quote_c_style_counted(name, -1, sb, fp, nodq);
}
