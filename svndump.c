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

/**
 * read a modified file (node) within a revision
 */ 
void  svnnode_read(char *fname)
{
    int type = NODEKIND_UNKNOWN;
    int action;
    int propLength = 0;
    int textLength = 0;
    char *src = NULL;
    char *fullSrcPath = NULL;
    char *t;
    char *val;
    t = svndump_read_line();
    
    do {
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
            src = &t[20];
        } else if (!strncmp(t, "Text-content-length:", 20)) {
            val = &t[21];
            textLength = atoi(val);
        } else if (!strncmp(t, "Prop-content-length:", 20)) {
            val = &t[21];
            propLength = atoi(val);
        }
        t = svndump_read_line();
    } while (strlen(t) && !feof(stdin));
    
        /* check if it's real add or possibly copy_or_move */ 
        if ((NULL != src) && (action == NODEACT_ADD)) {
        
            /* we don't really know at the moment */ 
            action = NODEACT_COPY_OR_MOVE;
    }
    if (propLength) {
        seek(propLength);
    }
    if (textLength) {
        copy_bytes(textLength);
    }
    t = svndump_read_line();
    if (!strlen(t))
        svndump_pushBackInputLine(t);
}
    
/**
 * Note: creating the revision will import it from
 * stdin
 */ 
    
int strendswith(char *s, char *end);

/**
 * create revision reading from stdin
 * param number revision number
 */ 
void  svnrev_read(uint32_t number)
{
    char *descr = "";
    char *author = "";
    char *date = "";
    char *t;
    int len;
    char *key = "";
    char *val = "";
    
        /* skip rest of revision definition */ 
        while (strlen(svndump_read_line()));
    
        /* key-value pairs containing log, date etc. */ 
        t = svndump_read_line();
    
    do {
        if (!strncmp(t, "K ", 2)) {
            len = atoi(&t[2]);
            key = svndump_read_string(len);
            svndump_read_line();
            t = svndump_read_line();
        } else if (!strncmp(t, "V ", 2)) {
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
    while (strncmp(t, "Revision-number:", 16) && !feof(stdin)) {
        if (!strncmp(t, "Node-path:", 10)) {
            svnnode_read(&t[11]);
        }
        
        do {
            t = svndump_read_line();
        } while ((!feof(stdin)) && (!strlen(t)));
    }
    if (strlen(t))
        svndump_pushBackInputLine(t);
}
