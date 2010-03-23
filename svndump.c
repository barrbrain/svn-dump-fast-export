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
 * node was moved to somwhere else
 * (this is not contained in the dump)
 * (only used in the FileChangeSet)
 */
#define NODEACT_REMOVE 5

/**
 * node was moved from somwhere else
 * (this is not contained in the dump)
 * (only used in the FileChangeSet)
 */
#define NODEACT_MOVE 4

/**
 * not clear if moved (if deleted afterwards) or
 * added as copy (which can not be modeled straight in ccase).
 * Will be used on create of SvnNodeEntry iff
 * action is add and source is given.
 * Will be modified after importing all files
 * of a revision to NODEACT_ADD (copy which can
 * not be modeled in ccase) or NODEACT_MOVE
 */
#define NODEACT_COPY_OR_MOVE 3

/**
 * node was deleted
 */
#define NODEACT_DELETE 2

/**
 * Node was added or copied from other location
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

/**
 * Node is a directory
 */
#define NODEKIND_DIR 1

/**
 * Node is a file
 */
#define NODEKIND_FILE 0

/**
 * unknown type of node
 */
#define NODEKIND_UNKNOWN -1

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
    int type = NODEKIND_UNKNOWN;
    int action = NODEACT_UNKNOWN;
    int propLength = 0;
    int textLength = 0;
    char *src = NULL;
    int srcRev = 0;
    char *dst = strdup(fname);
    char *t;
    char *val;
    uint32_t mark = 0;;

    fprintf(stderr, "Node path: %s\n", fname);

    for (t = svndump_read_line();
         t && *t;
         t = svndump_read_line()) {
        if (!strncmp(t, "Node-kind:", 10)) {
            val = &t[11];
            if (!strncasecmp(val, "dir", 3))
                type = NODEKIND_DIR;

            else if (!strncasecmp(val, "file", 4))
                type = NODEKIND_FILE;

            else
                type = NODEKIND_UNKNOWN;
        } else if (!strncmp(t, "Node-action", 11)) {
            val = &t[13];
            if (!strncasecmp(val, "delete", 6))
                action = NODEACT_DELETE;

            else if (!strncasecmp(val, "add", 3))
                action = NODEACT_ADD;

            else if (!strncasecmp(val, "change", 6))
                action = NODEACT_CHANGE;

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

    if (propLength) {
        skip_bytes(propLength);
    }
    if (textLength) {
        mark = next_blob_mark();
        printf("blob\nmark :%d\ndata %d\n", mark, textLength);
        copy_bytes(textLength);
        fputc('\n', stdout);
    }

    if (action == NODEACT_DELETE) {
        repo_delete(dst);
    } else if (action == NODEACT_CHANGE) {
        if (mark) {
            repo_modify(dst, mark);
        } else if (src && srcRev) {
            repo_copy(srcRev, src, dst);
        }
    } else if (action == NODEACT_ADD) {
        if (mark) {
            repo_add(dst,
                     type == NODEKIND_DIR ? REPO_MODE_DIR : REPO_MODE_BLB,
                     mark);
        } else if (src) {
            repo_copy(srcRev, src, dst);
        }
    }
}

/**
 * create revision reading from stdin
 * param number revision number
 */
static void svnrev_read(uint32_t number)
{
    struct tm tm;
    time_t timestamp;
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

    repo_commit(number);

    if (!number)
        return;

    printf("commit refs/heads/master\nmark :%d\n", number);
    printf("committer %s <%s@local> %d +0000\n",
         author, author, time(&timestamp));
    printf("data %d\n%s\n", strlen(descr), descr);
    repo_diff(number - 1, number);
    fputc('\n', stdout);

    printf("progress Imported commit %d.\n\n", number);
}

/*
 * create dump representation by importing dump file
 */
static void svndump_read(void)
{
    char *t;
    for (t = svndump_read_line(); t; t = svndump_read_line()) {
        if (!strncmp(t, "Revision-number:", 16)) {
            svnrev_read(atoi(&t[17]));
        }
    } 
}

int main(int argc, char **argv)
{
    svndump_read();
    return 0;
}
