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

/**
 * Note: creating the revision will import it from
 * stdin
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "svndump.h"

int strendswith(char* s, char* end);

/**
 * create revision reading from stdin
 * param number revision number
 */
void
svnrev_read(int number) {
    char* descr = "";
    char* author = "";
    char* date = "";
    char *t;
    int len;
    char* key = "";
    char* val = "";

    /* skip rest of revision definition */
    while (strlen(svndump_read_line()));
    /* key-value pairs containing log, date etc. */
    t = svndump_read_line();
    do {
        if (!strncmp(t,"K ", 2)) {
            len = atoi(&t[2]);
            key = svndump_read_string(len);
            svndump_read_line();
            t = svndump_read_line();
        } else if (!strncmp(t,"V ",2)) {
            len = atoi(&t[2]);
            val = svndump_read_string(len);

            if (strendswith(key, ":log"))
            {
                descr = val;
            } else if (strendswith(key, ":author")) {
                author = val;
            } else if (strendswith(key, ":date")) {
                date = val;
            }
            key = "";
            svndump_read_line();
            t = svndump_read_line();
        }
    } while (strlen(t) && strncasecmp(t, "PROPS-END", 9));

    do {
        t = svndump_read_line();
    } while ((!feof(stdin)) && (!strlen(t)));

    while (strncmp(t,"Revision-number:", 16) && !feof(stdin)) {
        if (!strncmp(t,"Node-path:",10)) {
            svnnode_read(&t[11]);
        }
        do {
            t = svndump_read_line();
        } while ((!feof(stdin)) && (!strlen(t)));
    }
    if (strlen(t))
        svndump_pushBackInputLine(t);
}
