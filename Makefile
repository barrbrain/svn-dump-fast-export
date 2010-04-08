svn-dump-fast-export: *.c *.h Makefile
	$(CC) *.c -o svn-dump-fast-export -O2 -Wall
