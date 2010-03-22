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

#ifndef SVNDUMP_H_
#define SVNDUMP_H_

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

char *svndump_read_line(void);

void svndump_read(void);

void svnrev_read(uint32_t revision_id);

#endif
