.PHONY: default clean
default: svn-fe-dbg
%.o: %.c
	$(CC) -c *.c -I.
svn-fe: *.o
	$(CC) *.o -o svn-fe -O2
svn-fe-dbg: *.o
	$(CC) *.c -o svn-fe-dbg -O1 -Wall -ggdb
clean:
	$(RM) *.o svn-fe svn-fe-dbg
