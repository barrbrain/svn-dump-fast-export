#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "svndump.h"

int main(int argc, char **argv)
{
    repo_init(30000, 100000, 100, 100000);
    svndump_read();
    return 0;
}
