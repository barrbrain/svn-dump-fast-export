/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "compat-util.h"
#include "line_buffer.h"
#include "strbuf.h"

#define COPY_BUFFER_LEN 4096

static int wrap_fread(void *stream, char *buffer, size_t len)
{
	return fread(buffer, 1, len, (FILE*)stream);
}

static int wrap_fwrite(void *stream, const char *buffer, size_t len)
{
	return fwrite(buffer, 1, len, (FILE*)stream);
}

static int noop_close(void *stream)
{
	stream = NULL;
	return 0;
}

static int wrap_file(struct line_buffer *buf, FILE *file)
{
	if (!file)
		return -1;
	buf->line_alloc = 0;
	buf->line_off = 0;
	buf->stream = file;
	buf->read = &wrap_fread;
	buf->write = &wrap_fwrite;
	buf->close = file == stdin ? noop_close : (int (*)(void *))&fclose;
	buf->error = (int (*)(void *))&ferror;
	buf->free = (void (*)(void *))&free;
	return 0;
}

int buffer_init(struct line_buffer *buf, const char *filename)
{
	return wrap_file(buf, filename ? fopen(filename, "r") : stdin);
}

int buffer_fdinit(struct line_buffer *buf, int fd)
{
	return wrap_file(buf, fdopen(fd, "r"));
}

int buffer_tmpfile_init(struct line_buffer *buf)
{
	return wrap_file(buf, tmpfile());
}

int buffer_deinit(struct line_buffer *buf)
{
	int err;
	err = (buf->error)(buf->stream);
	err |= (buf->close)(buf->stream);
	return err;
}

FILE *buffer_tmpfile_rewind(struct line_buffer *buf)
{
	FILE *infile = (FILE*)buf->stream;
	rewind(infile);
	return infile;
}

long buffer_tmpfile_prepare_to_read(struct line_buffer *buf)
{
	FILE *infile = (FILE*)buf->stream;
	long pos = ftell(infile);
	if (pos < 0)
		return error("ftell error: %s", strerror(errno));
	if (fseek(infile, 0, SEEK_SET))
		return error("seek error: %s", strerror(errno));
	return pos;
}

int buffer_ferror(struct line_buffer *buf)
{
	return (buf->error)(buf->stream);
}

static int buffer_read(struct line_buffer *buf, char *out, size_t size)
{
	int ret = 0;
	size_t n = buf->line_alloc > size ? size : buf->line_alloc;
	if (n) {
		memcpy(out, buf->line_buffer + buf->line_off, n);
		size -= n;
		out += n;
		buf->line_alloc -= n;
		buf->line_off += n;
	}
	if (size)
		ret = (buf->read)(buf->stream, out, size);
	return ret + n;
}

int buffer_read_char(struct line_buffer *buf)
{
	unsigned char c;
	return buffer_read(buf, (char *)&c, 1) == 1 ? (int)c : EOF;
}

static char *buffer_gets(struct line_buffer *buf)
{
	if (buf->line_alloc) {
		char *ret = buf->line_buffer + buf->line_off;
		char *eol = memchr(ret, '\n', buf->line_alloc);
		if (eol) {
			*eol++ = '\0';
			buf->line_off += eol - ret;
			buf->line_alloc -= eol - ret;
			return ret;
		}
		memmove(buf->line_buffer, ret, buf->line_alloc);
	}
	buf->line_off = 0;
	while (buf->line_alloc < LINE_BUFFER_LEN - 1) {
		char *out = buf->line_buffer + buf->line_alloc;
		int size = 1;
		int n = (buf->read)(buf->stream, out, size);
		char *eol = memchr(out, '\n', n);
		buf->line_alloc += n;
		buf->line_buffer[buf->line_alloc] = '\0';
		if (eol) {
			*eol++ = '\0';
			buf->line_off = eol - buf->line_buffer;
			buf->line_alloc -= buf->line_off;
			return buf->line_buffer;
		}
		if (n < size)
			return buf->line_buffer;
	}
	/* error on long lines */
	buf->line_alloc = 0;
	return NULL;
}

/* Read a line without trailing newline. */
char *buffer_read_line(struct line_buffer *buf)
{
	char *s = buffer_gets(buf);
	//fprintf(stderr, "%s\n", s);
	return s;
}

size_t buffer_read_binary(struct line_buffer *buf,
				struct strbuf *sb, size_t size)
{
	size_t res;
	strbuf_grow(sb, size);
	res = buffer_read(buf, sb->buf + sb->len, size);
	//fwrite(sb->buf + sb->len, 1, res, stderr);
	//fputc('\n', stderr);
	if (res > 0)
                strbuf_setlen(sb, sb->len + res);
	return res;
}

off_t buffer_copy_bytes(struct line_buffer *buf, off_t nbytes)
{
	char byte_buffer[COPY_BUFFER_LEN];
	off_t done = 0;
	while (done < nbytes && !(buf->error)(buf->stream)) {
		off_t len = nbytes - done;
		size_t in = len < COPY_BUFFER_LEN ? len : COPY_BUFFER_LEN;
		in = buffer_read(buf, byte_buffer, in);
		done += in;
		fwrite(byte_buffer, 1, in, stdout);
		if (ferror(stdout))
			return done + buffer_skip_bytes(buf, nbytes - done);
	}
	return done;
}

off_t buffer_skip_bytes(struct line_buffer *buf, off_t nbytes)
{
	char byte_buffer[COPY_BUFFER_LEN];
	off_t done = 0;
	while (done < nbytes && !(buf->error)(buf->stream)) {
		off_t len = nbytes - done;
		size_t in = len < COPY_BUFFER_LEN ? len : COPY_BUFFER_LEN;
		done += buffer_read(buf, byte_buffer, in);
	}
	return done;
}
