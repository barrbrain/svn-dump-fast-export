/*
 * From git 1.7.10.2
 * License: GPL-2
 *
 * Modifications (2012-05-18):
 *  - add license header.
 *  - remove unneeded functions.
 */

#ifndef QUOTE_H
#define QUOTE_H

struct strbuf;

extern size_t quote_c_style(const char *name, struct strbuf *, FILE *, int no_dq);

#endif
