#ifndef LINE_BUFFER_H_
#define LINE_BUFFER_H_

char *buffer_read_line(void);

char *buffer_read_string(int len);

void buffer_copy_bytes(int len);

void buffer_skip_bytes(int len);

#endif
