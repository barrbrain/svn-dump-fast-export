#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "repo_tree.h"

int main(int argc, char **argv)
{
    uint32_t rev;
    char *cmd;
    char *a1, *a2, *a3;
    char buffer[4096];
    uint32_t blob_mark = 1000000000;
    repo_init(30000, 100000, 100, 100000);
    pool_init(300000, 30000);
    for (fgets(buffer, 4096, stdin);
         !feof(stdin); fgets(buffer, 4096, stdin)) {

        cmd = strtok(buffer, " ");
        if (*cmd == 'C') {
            a1 = strtok(NULL, ":");
            a2 = strtok(NULL, " ");
            a3 = strtok(NULL, "\n");
            repo_copy(atoi(a1), a2, a3);
        } else if (*cmd == 'A') {
            a1 = strtok(NULL, "\n");
            repo_add(a1, blob_mark++);
        } else if (*cmd == 'M') {
            a1 = strtok(NULL, "\n");
            repo_modify(a1, blob_mark++);
        } else if (*cmd == 'D') {
            a1 = strtok(NULL, "\n");
            repo_delete(a1);
        } else if (*cmd == 'R') {
            a1 = strtok(NULL, "\n");
            rev = atoi(a1);
            repo_commit(rev);
            if (rev)
                repo_diff(rev - 1, rev);
        }
    }
    return 0;
}
