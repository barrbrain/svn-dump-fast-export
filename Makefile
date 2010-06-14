.PHONY: default clean
default: svn-fe-dbg
%.o: %.c *.h
	$(CC) -c *.c -I. -Wall -Werror -O0 -ggdb3
svn-fe: *.c *.h
	$(CC) *.c -o $@ -O2
svn-fe-dbg: *.o
	$(CC) *.o -o $@
clean:
	$(RM) *.o svn-fe svn-fe-dbg
