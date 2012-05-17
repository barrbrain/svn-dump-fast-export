#ifndef SVNDUMP_H_
#define SVNDUMP_H_

void svndump_init(const char *filename);
void svndump_deinit(void);
void svndump_read(char *url);
void svndump_reset(void);

#endif
