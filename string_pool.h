#ifndef STRING_POOL_H_
#define	STRING_POOL_H_

#include <stdint.h>
#include <stdio.h>

uint32_t pool_tok_r(char *str, const char *delim, char **saveptr);
void pool_print_seq(uint32_t len, uint32_t *seq, char delim, FILE *stream);
uint32_t pool_tok_seq(uint32_t max, uint32_t *seq, char *delim, char *str);
void pool_reset(void);

#endif
