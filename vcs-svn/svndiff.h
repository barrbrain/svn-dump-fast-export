#ifndef SVNDIFF_H_
#define SVNDIFF_H_

#include "line_buffer.h"

extern int svndiff0_apply(struct line_buffer *delta, off_t delta_len,
			  struct line_buffer *preimage, FILE *postimage);

#endif
