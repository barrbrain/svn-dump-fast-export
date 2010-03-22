svn-dump-fast-export: *.c *.h
	$(CC) *.c -o svn-dump-fast-export -O2
