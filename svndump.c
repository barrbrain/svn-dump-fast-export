/******************************************************************************
 *
 * Copyright (C) 2005 Stefan Hegny, hydrografix Consulting GmbH,
 * Frankfurt/Main, Germany
 * and others, see http://svn2cc.sarovar.org
 *
 * Copyright (C) 2010 David Barr <david.barr@cordelta.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer
 *    unmodified other than the allowable addition of one or more
 *    copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
  ******************************************************************************/

/*
 * Parse and rearrange a svnadmin dump.
 * Create the dump with:
 * svnadmin dump --incremental -r<startrev>:<endrev> <repository> >outfile
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "svndump.h"
#include "svnrev.h"

/*
 * Date format:
 * "yyyy-MM-dd'T'HH:mm:ss"
 */

/*
 * create dump representation by importing dump file
 */
void svndump_read(void)
{
    char *t = svndump_read_line();
    while (t && strncmp(t, "Revision-number:", 16))
        t = svndump_read_line();

    do {
        svnrev_read(atoi(&t[17]));
        t = svndump_read_line();
    } while (strlen(t) && !feof(stdin));
}

/*
 * read string up to newline from input stream
 * return all characters except the newline
 */
static char line_buffer[10000];

char *svndump_read_line(void)
{
    int len;
    char *res = fgets(line_buffer, 10000, stdin);

    if (res) {
        len = strlen(res);

        if (res[len - 1] == '\n')
            res[len - 1] = '\0';
    }
    return res;
}


/*
 * so a line can be pushed-back after read
 */
static char *lastLine = NULL;

void svndump_pushBackInputLine(char *input)
{
    lastLine = input;
}
