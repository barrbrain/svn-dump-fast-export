/******************************************************************************
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "string_pool.h"

int main(int argc, char **argv)
{
    char *cmd;
    char *a1, *a2, *a3;
    char buffer[4096];
    pool_init(300000, 30000);
    for (fgets(buffer, 4096, stdin);
         !feof(stdin); fgets(buffer, 4096, stdin)) {

        cmd = strtok(buffer, " ");
        if (!strcmp(cmd, "copy")) {
            a1 = strtok(NULL, ":");
            a2 = strtok(NULL, "\"");
            strtok(NULL, "\"");
            a3 = strtok(NULL, "\"");
            printf("%s %s:\"", cmd, a1);
            for (a2 = strtok(a2, "/"); a2;
                 (a2 = strtok(NULL, a2)) && putchar('/')) {
                printf("%d", pool_intern(a2));
            }
            printf("\" \"");
            for (a3 = strtok(a3, "/"); a3;
                 (a3 = strtok(NULL, a3)) && putchar('/')) {
                printf("%d", pool_intern(a3));
            }
            printf("\"\n");
        } else if (!strcmp(cmd, "add") || !strcmp(cmd, "modify")
                   || !strcmp(cmd, "delete")) {
            a1 = strtok(NULL, "\"");
            strtok(NULL, "\"");
            printf("%s \"", cmd);
            for (a1 = strtok(a1, "/"); a1;
                 (a1 = strtok(NULL, a1)) && putchar('/')) {
                printf("%d", pool_intern(a1));
            }
            printf("\"\n");
        } else if (!strcmp(cmd, "commit")) {
            a1 = strtok(NULL, "\n");
            printf("%s %s\n", cmd, a1);
        }
    }
    return 0;
}
