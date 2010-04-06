#ifndef STRING_POOL_H_
#define	STRING_POOL_H_

uint32_t pool_intern(char *key);

uint32_t pool_tok_r(char *str, const char *delim, char **saveptr);

void pool_print_seq(uint32_t len, uint32_t * seq, char delim, FILE * stream);

#endif
