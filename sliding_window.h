#ifndef SLIDING_WINDOW_H_
#define SLIDING_WINDOW_H_

#include "strbuf.h"

struct view {
	struct line_buffer *file;
	off_t off;
	struct strbuf buf;
};

extern int move_window(struct view *view, off_t off, size_t len);

#endif
