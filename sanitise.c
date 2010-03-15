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
        } else if (!strcmp(cmd, "add") || !strcmp(cmd, "modify") || !strcmp(cmd, "delete")) {
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
