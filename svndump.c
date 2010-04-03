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
#include "line_buffer.h"

#define NODEACT_REPLACE 4
#define NODEACT_DELETE 3
#define NODEACT_ADD 2
#define NODEACT_CHANGE 1
#define NODEACT_UNKNOWN 0

#define DUMP_CTX 0
#define REV_CTX  1
#define NODE_CTX 2

#define LENGTH_UNKNOWN (~0)

#define BLOB_MARK_OFFSET 1000000000

static struct {
    uint32_t action, propLength, textLength, srcRev, srcMode, mark, type;
    char *src, *dst;
} node_ctx;

static struct {
    uint32_t revision;
    time_t timestamp;
    char *descr, *author, *date;
} rev_ctx;

static struct {
    char *uuid, *url;
} dump_ctx;

static void reset_node_ctx(char * fname)
{
    node_ctx.type = 0;
    node_ctx.action = NODEACT_UNKNOWN;
    node_ctx.propLength = LENGTH_UNKNOWN;
    node_ctx.textLength = LENGTH_UNKNOWN;
    node_ctx.src = NULL;
    node_ctx.srcRev = 0;
    node_ctx.srcMode = 0;
    node_ctx.dst = strdup(fname);
    node_ctx.mark = 0;
}

static void reset_rev_ctx(uint32_t revision)
{
    rev_ctx.revision = revision;
    rev_ctx.timestamp = 0;
    rev_ctx.descr = "";
    rev_ctx.author = "nobody";
    rev_ctx.date = "now";
}

static void reset_dump_ctx(char * url) {
    dump_ctx.url = url;
    dump_ctx.uuid = NULL;
}

static uint32_t next_blob_mark(void)
{
    static uint32_t mark = BLOB_MARK_OFFSET;
    return mark++;
}

static void read_props(void)
{
    struct tm tm;
    uint32_t len;
    char *key = "";
    char *val = "";
    char *t;
    while ((t = buffer_read_line()) && strcmp(t, "PROPS-END")) {
        if (!strncmp(t, "K ", 2)) {
            len = atoi(&t[2]);
            key = buffer_read_string(len);
            buffer_read_line();
        } else if (!strncmp(t, "V ", 2)) {
            len = atoi(&t[2]);
            val = buffer_read_string(len);
            if (!strcmp(key, "svn:log")) {
                rev_ctx.descr = val;
            } else if (!strcmp(key, "svn:author")) {
                rev_ctx.author = val;
            } else if (!strcmp(key, "svn:date")) {
                rev_ctx.date = val;
                strptime(rev_ctx.date, "%FT%T", &tm);
                timezone = 0;
                tm.tm_isdst = 0;
                rev_ctx.timestamp = mktime(&tm);
            } else if (!strcmp(key, "svn:executable")) {
                if (node_ctx.type == REPO_MODE_BLB) {
                    node_ctx.type = REPO_MODE_EXE;
                }
            } else if (!strcmp(key, "svn:special")) {
                if (node_ctx.type == REPO_MODE_BLB) {
                    node_ctx.type = REPO_MODE_LNK;
                }
            }
            key = "";
            buffer_read_line();
        }
    }
}

static void handle_node(void)
{
    if (node_ctx.propLength != LENGTH_UNKNOWN && node_ctx.propLength) {
        read_props();
    }

    if (node_ctx.src && node_ctx.srcRev) {
        node_ctx.srcMode =
            repo_copy(node_ctx.srcRev, node_ctx.src, node_ctx.dst);
    }

    if (node_ctx.textLength != LENGTH_UNKNOWN &&
        node_ctx.type != REPO_MODE_DIR) {
        node_ctx.mark = next_blob_mark();
    }

    if (node_ctx.action == NODEACT_DELETE) {
        repo_delete(node_ctx.dst);
    } else if (node_ctx.action == NODEACT_CHANGE || 
               node_ctx.action == NODEACT_REPLACE) {
        if (node_ctx.propLength != LENGTH_UNKNOWN &&
            node_ctx.textLength != LENGTH_UNKNOWN) {
            repo_modify(node_ctx.dst, node_ctx.type, node_ctx.mark);
        } else if (node_ctx.textLength >= 0) {
            node_ctx.srcMode = repo_replace(node_ctx.dst, node_ctx.mark);
        }
    } else if (node_ctx.action == NODEACT_ADD) {
        if (node_ctx.src && node_ctx.srcRev &&
            node_ctx.propLength == LENGTH_UNKNOWN &&
            node_ctx.textLength != LENGTH_UNKNOWN) {
            node_ctx.srcMode = repo_replace(node_ctx.dst, node_ctx.mark);
        } else if (node_ctx.type == REPO_MODE_DIR ||
                   node_ctx.textLength != LENGTH_UNKNOWN){
            repo_add(node_ctx.dst, node_ctx.type, node_ctx.mark);
        }
    }

    if (node_ctx.propLength == LENGTH_UNKNOWN && node_ctx.srcMode) {
        node_ctx.type = node_ctx.srcMode;
    }

    if (node_ctx.mark) {
        repo_copy_blob(node_ctx.type, node_ctx.mark, node_ctx.textLength);
    } else if (node_ctx.textLength != LENGTH_UNKNOWN) {
        buffer_skip_bytes(node_ctx.textLength);
    }
}

static void handle_revision(void)
{
    repo_commit(rev_ctx.revision, rev_ctx.author, rev_ctx.descr, dump_ctx.uuid,
                dump_ctx.url, rev_ctx.timestamp);
}

static void svndump_read(char * url)
{
    char *val;
    char *t;
    uint32_t active_ctx = DUMP_CTX; 
    uint32_t len;

    reset_dump_ctx(url);
    while (t = buffer_read_line()) {
        val = strstr(t, ": ");
        if (!val) continue;
        *val++ = '\0';
        *val++ = '\0';

        if(!strcmp(t, "UUID")) {
            dump_ctx.uuid = strdup(val);
        } else if (!strcmp(t, "Revision-number")) {
            if (active_ctx != DUMP_CTX) handle_revision();
            active_ctx = REV_CTX;
            reset_rev_ctx(atoi(val));
        } else if (!strcmp(t, "Node-path")) {
            active_ctx = NODE_CTX;
            reset_node_ctx(val);
        } else if (!strcmp(t, "Node-kind")) {
            if (!strcmp(val, "dir")) {
                node_ctx.type = REPO_MODE_DIR;
            } else if (!strcmp(val, "file")) {
                node_ctx.type = REPO_MODE_BLB;
            } else {
                fprintf(stderr, "Unknown node-kind: %s\n", val);
            }
        } else if (!strcmp(t, "Node-action")) {
            if (!strcmp(val, "delete")) {
                node_ctx.action = NODEACT_DELETE;
            } else if (!strcmp(val, "add")) {
                node_ctx.action = NODEACT_ADD;
            } else if (!strcmp(val, "change")) {
                node_ctx.action = NODEACT_CHANGE;
            } else if (!strcmp(val, "replace")) {
                node_ctx.action = NODEACT_REPLACE;
            } else {
                fprintf(stderr, "Unknown node-action: %s\n", val);
                node_ctx.action = NODEACT_UNKNOWN;
            }
        } else if (!strcmp(t, "Node-copyfrom-path")) {
            node_ctx.src = strdup(val);
        } else if (!strcmp(t, "Node-copyfrom-rev")) {
            node_ctx.srcRev = atoi(val);
        } else if (!strcmp(t, "Text-content-length")) {
            node_ctx.textLength = atoi(val);
        } else if (!strcmp(t, "Prop-content-length")) {
            node_ctx.propLength = atoi(val);
        } else if (!strcmp(t, "Content-length")) {
            len = atoi(val);
            buffer_read_line();
            if (active_ctx == REV_CTX) {
                read_props();
            } else if (active_ctx == NODE_CTX) {
                handle_node();
                active_ctx = REV_CTX;
            } else {
                fprintf(stderr, "Unexpected content length header: %d\n", len);
                buffer_skip_bytes(len);
            }
        }
    } 
    if (active_ctx != DUMP_CTX) handle_revision();
}

int main(int argc, char **argv)
{
    svndump_read((argc > 1) ? argv[1] : NULL);
    return 0;
}
