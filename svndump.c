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
#include <stdlib.h>
#include <time.h>
#include "repo_tree.h"

/**
 * node was replaced
 */
#define NODEACT_REPLACE 3

/**
 * node was deleted
 */
#define NODEACT_DELETE 2

/**
 * node was added or copied from other location
 */
#define NODEACT_ADD 1

/**
 * node was modified
 */
#define NODEACT_CHANGE 0

/**
 * unknown action
 */
#define NODEACT_UNKNOWN -1

static char line_buffer[10000];
static int line_buffer_len = 0;
static int line_len = 0;

/*
 * read string up to newline from input stream
 * return all characters except the newline
 */
static char *svndump_read_line(void)
{
    char *res;
    char *end;
    int n_read;

    if (line_len) {
        memmove(line_buffer, &line_buffer[line_len],
                line_buffer_len - line_len);
        line_buffer_len -= line_len;
        line_len = 0;
    }

    end = memchr(line_buffer, '\n', line_buffer_len);
    while (line_buffer_len < 9999 && !feof(stdin) && NULL == end) {
        n_read =
            fread(&line_buffer[line_buffer_len], 1, 9999 - line_buffer_len,
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

/*
 * so a line can be pushed-back after read
 */
static void svndump_pushBackInputLine()
{
    if (line_len) {
        if (line_buffer[line_len - 1] == '\0')
            line_buffer[line_len - 1] = '\n';
        line_buffer[line_buffer_len] = '\0';
        line_len = 0;
    }
}

static char *svndump_read_string(int len)
{
    char *s = malloc(len + 1);
    int offset = 0;
    if (line_buffer_len > line_len) {
        offset = line_buffer_len - line_len;
        if (offset > len)
            offset = len;
        memcpy(s, &line_buffer[line_len], offset);
        line_len += offset;
    }
    while (offset < len && !feof(stdin)) {
        offset += fread(&s[offset], 1, len - offset, stdin);
    }
    s[offset] = '\0';
    return s;
}

char byte_buffer[4096];
static void copy_bytes(int len)
{
    int in, out;
    if (line_buffer_len > line_len) {
        in = line_buffer_len - line_len;
        if (in > len)
            in = len;
        out = 0;
        while (out < in && !ferror(stdout)) {
            out +=
                fwrite(&line_buffer[line_len + out], 1, in - out, stdout);
        }
        len -= in;
        line_len += in;
    }
    while (len > 0 && !feof(stdin)) {
        in = len < 4096 ? len : 4096;
        in = fread(byte_buffer, 1, in, stdin);
        len -= in;
        out = 0;
        while (out < in && !ferror(stdout)) {
            out += fwrite(&byte_buffer[out], 1, in - out, stdout);
        }
    }
}

static void skip_bytes(int len)
{
    int in;
    if (line_buffer_len > line_len) {
        in = line_buffer_len - line_len;
        if (in > len)
            in = len;
        line_len += in;
        len -= in;
    }
    while (len > 0 && !feof(stdin)) {
        in = len < 4096 ? len : 4096;
        in = fread(byte_buffer, 1, in, stdin);
        len -= in;
    }
}

static int strendswith(char *s, char *end)
{
    int end_len = strlen(end);
    int s_len = strlen(s);
    return s_len >= end_len && !strcmp(&s[s_len - end_len], end);
}

static uint32_t next_blob_mark(void)
{
    static int32_t mark = 1000000000;
    return mark++;
}

/**
 * read a modified file (node) within a revision
 */
static void svnnode_read(char *fname)
{
    int type = 0;
    int action = NODEACT_UNKNOWN;
    int propLength = -1;
    int textLength = -1;
    char *src = NULL;
    int srcRev = 0;
    char *dst = strdup(fname);
    char *t;
    int len;
    char *key;
    char *val;
    uint32_t mark = 0;

    fprintf(stderr, "Node path: %s\n", fname);

    for (t = svndump_read_line();
         t && *t;
         t = svndump_read_line()) {
        if (!strncmp(t, "Node-kind:", 10)) {
            val = &t[11];
            if (!strncasecmp(val, "dir", 3)) {
                type = REPO_MODE_DIR;
            } else if (!strncasecmp(val, "file", 4)) {
                type = REPO_MODE_BLB;
            } else {
                fprintf(stderr, "Unknown node-kind: %s\n", val);
            }
        } else if (!strncmp(t, "Node-action", 11)) {
            val = &t[13];
            if (!strncasecmp(val, "delete", 6))
                action = NODEACT_DELETE;

            else if (!strncasecmp(val, "add", 3))
                action = NODEACT_ADD;

            else if (!strncasecmp(val, "change", 6))
                action = NODEACT_CHANGE;

            else if (!strncasecmp(val, "replace", 6))
                action = NODEACT_REPLACE;

            else
                action = NODEACT_UNKNOWN;
        } else if (!strncmp(t, "Node-copyfrom-path", 18)) {
            src = strdup(&t[20]);
            fprintf(stderr, "Node copy path: %s\n", src);
        } else if (!strncmp(t, "Node-copyfrom-rev", 17)) {
            val = &t[19];
            srcRev = atoi(val);
            fprintf(stderr, "Node copy revision: %d\n", srcRev);
        } else if (!strncmp(t, "Text-content-length:", 20)) {
            val = &t[21];
            textLength = atoi(val);
            fprintf(stderr, "Text content length: %d\n", textLength);
        } else if (!strncmp(t, "Prop-content-length:", 20)) {
            val = &t[21];
            propLength = atoi(val);
            fprintf(stderr, "Prop content length: %d\n", propLength);
        }
    }

    if (propLength > 0) {
        for (t = svndump_read_line();
             t && strncasecmp(t, "PROPS-END", 9);
             t = svndump_read_line()) {
            if (!strncmp(t, "K ", 2)) {
                len = atoi(&t[2]);
                key = svndump_read_string(len);
                svndump_read_line();
            } else if (!strncmp(t, "V ", 2)) {
                len = atoi(&t[2]);
                val = svndump_read_string(len);
                if (strendswith(key, ":executable")) {
                    if (type == REPO_MODE_BLB) {
                        type = REPO_MODE_EXE;
                    }
                    fprintf(stderr, "Executable: %s\n", val);
                } else if (strendswith(key, ":special")) {
                    if (type == REPO_MODE_BLB) {
                        type = REPO_MODE_LNK;
                    }
                    fprintf(stderr, "Special: %s\n", val);
                }
                key = "";
                svndump_read_line();
            }
        }
    }

    if (src && srcRev) {
        repo_copy(srcRev, src, strdup(dst));
    }

    if (textLength >= 0 && type != REPO_MODE_DIR) {
        mark = next_blob_mark();
    }

    if (action == NODEACT_DELETE) {
        repo_delete(dst);
    } else if (action == NODEACT_CHANGE || 
               action == NODEACT_REPLACE) {
        if (propLength >= 0 && textLength >= 0) {
            repo_modify(dst, type, mark);
        } else if (textLength >= 0) {
            repo_replace(dst, mark);
        }
    } else if (action == NODEACT_ADD) {
        if (src && srcRev && propLength < 0 && textLength >= 0) {
            repo_replace(dst, mark);
        } else if(type == REPO_MODE_DIR || textLength >= 0){
            repo_add(dst, type, mark);
        }
    }

    if(textLength == -1) textLength = 0;

    if (mark) {
        if (type == REPO_MODE_LNK) {
            /* svn symlink blobs start with "link " */
            skip_bytes(5);
            textLength -= 5;
        }
        printf("blob\nmark :%d\ndata %d\n", mark, textLength);
        copy_bytes(textLength);
        fputc('\n', stdout);
    } else {
        skip_bytes(textLength);
    }
}

static char *uuid = NULL;
static char *url = NULL;


/**
 * create revision reading from stdin
 * param number revision number
 */
static void svnrev_read(uint32_t number)
{
    struct tm tm;
    time_t timestamp = 0;
    char *descr = "";
    char *author = "nobody";
    char *date = "now";
    char *t;
    int len;
    char *key = "";
    char *val = "";

    fprintf(stderr, "Revision: %d\n", number);

    for (t = svndump_read_line();
         t && strncasecmp(t, "PROPS-END", 9);
         t = svndump_read_line()) {
        if (!strncmp(t, "K ", 2)) {
            len = atoi(&t[2]);
            key = svndump_read_string(len);
            svndump_read_line();
        } else if (!strncmp(t, "V ", 2)) {
            len = atoi(&t[2]);
            val = svndump_read_string(len);
            if (strendswith(key, ":log")) {
                descr = val;
                fprintf(stderr, "Log: %s\n", descr);
            } else if (strendswith(key, ":author")) {
                author = val;
                fprintf(stderr, "Author: %s\n", author);
            } else if (strendswith(key, ":date")) {
                date = val;
                fprintf(stderr, "Date: %s\n", date);
                strptime(date, "%FT%T", &tm);
                timezone = 0;
                tm.tm_isdst = 0;
                timestamp = mktime(&tm);
            }
            key = "";
            svndump_read_line();
        }
    }

    for ( ;
         t && strncmp(t, "Revision-number:", 16);
         t = svndump_read_line()) {
        if (!strncmp(t, "Node-path:", 10)) {
            svnnode_read(&t[11]);
        }
    }
    if (t)
        svndump_pushBackInputLine();

    repo_commit(number, author, descr, uuid, url, timestamp);
}

/*
 * create dump representation by importing dump file
 */
static void svndump_read(void)
{
    char *t;
    int revision;
    for (t = svndump_read_line(); t; t = svndump_read_line()) {
        if (!strncmp(t, "Revision-number:", 16)) {
            revision = atoi(&t[17]);
            svnrev_read(revision);
        } else if(!strncmp(t, "UUID:", 5)) {
            uuid = strdup(&t[6]);
        }
    } 
}

int main(int argc, char **argv)
{
    if (argc > 1) url = argv[1];
    svndump_read();
    return 0;
}
