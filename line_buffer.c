#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "line_buffer.h"
#include "obj_pool.h"

#define LINE_BUFFER_LEN 10000
#define COPY_BUFFER_LEN 4096

/* Create memory pool for char sequence of known length */
obj_pool_gen(blob, char, 4096);

static char line_buffer[LINE_BUFFER_LEN];
static char byte_buffer[COPY_BUFFER_LEN];
static uint32_t line_buffer_len = 0;
static uint32_t line_len = 0;

char *buffer_read_line(void)
{
	char *end;
	uint32_t n_read;

	if (line_len) {
		memmove(line_buffer, &line_buffer[line_len],
		                line_buffer_len - line_len);
		line_buffer_len -= line_len;
		line_len = 0;
	}

	end = memchr(line_buffer, '\n', line_buffer_len);
	while (line_buffer_len < LINE_BUFFER_LEN - 1 &&
		   !feof(stdin) && !ferror(stdin) && NULL == end) {
		n_read = fread(&line_buffer[line_buffer_len], 1,
				LINE_BUFFER_LEN - 1 - line_buffer_len,
				stdin);
		end = memchr(&line_buffer[line_buffer_len], '\n', n_read);
		line_buffer_len += n_read;
	}

	if (ferror(stdin))
		return NULL;

	if (end != NULL) {
		line_len = end - line_buffer;
		line_buffer[line_len++] = '\0';
	} else {
		line_len = line_buffer_len;
		line_buffer[line_buffer_len] = '\0';
	}

	if (line_len == 0)
		return NULL;

	return line_buffer;
}

char *buffer_read_string(uint32_t len)
{
	char *s;
	blob_free(blob_pool.size);
	s = blob_pointer(blob_alloc(len + 1));
	uint32_t offset = 0;
	if (line_buffer_len > line_len) {
		offset = line_buffer_len - line_len;
		if (offset > len)
			offset = len;
		memcpy(s, &line_buffer[line_len], offset);
		line_len += offset;
	}
	if (offset < len)
		offset += fread(&s[offset], 1, len - offset, stdin);
	s[offset] = '\0';
	return s;
}

void buffer_copy_bytes(uint32_t len)
{
	uint32_t in;
	if (line_buffer_len > line_len) {
		in = line_buffer_len - line_len;
		if (in > len)
			in = len;
		fwrite(&line_buffer[line_len], 1, in, stdout);
		len -= in;
		line_len += in;
	}
	while (len > 0 && !feof(stdin)) {
		in = len < COPY_BUFFER_LEN ? len : COPY_BUFFER_LEN;
		in = fread(byte_buffer, 1, in, stdin);
		len -= in;
		fwrite(byte_buffer, 1, in, stdout);
	}
}

void buffer_skip_bytes(uint32_t len)
{
	uint32_t in;
	if (line_buffer_len > line_len) {
		in = line_buffer_len - line_len;
		if (in > len)
			in = len;
		line_len += in;
		len -= in;
	}
	while (len > 0 && !feof(stdin) && !ferror(stdin)) {
		in = len < COPY_BUFFER_LEN ? len : COPY_BUFFER_LEN;
		in = fread(byte_buffer, 1, in, stdin);
		len -= in;
	}
}

void buffer_reset(void)
{
	blob_reset();
}
