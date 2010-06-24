/*
 * Parse and rearrange a svnadmin dump.
 * Create the dump with:
 * svnadmin dump --incremental -r<startrev>:<endrev> <repository> >outfile
 *
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include <stdlib.h>
#include "svndump.h"

int main(int argc, char **argv)
{
	svndump_init(NULL);
	svndump_read((argc > 1) ? argv[1] : NULL);
	svndump_reset();
	return 0;
}
