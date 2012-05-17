.PHONY: all clean
CFLAGS = -Wall -W -g -O2 -I. -Ivcs-svn
HEADERS = mkgmtime.h \
	strbuf.h \
	vcs-svn/compat-util.h \
	vcs-svn/fast_export.h \
	vcs-svn/line_buffer.h \
	vcs-svn/obj_pool.h \
	vcs-svn/repo_tree.h \
	vcs-svn/sliding_window.h \
	vcs-svn/string_pool.h \
	vcs-svn/svndiff.h \
	vcs-svn/svndump.h \
	vcs-svn/trp.h

OBJECTS = mkgmtime.o \
	strbuf.o \
	contrib/svn-fe/svn-fe.o \
	vcs-svn/fast_export.o \
	vcs-svn/line_buffer.o \
	vcs-svn/repo_tree.o \
	vcs-svn/sliding_window.o \
	vcs-svn/string_pool.o \
	vcs-svn/svndiff.o \
	vcs-svn/svndump.o

all: contrib/svn-fe/svn-fe
%.o: %.c $(HEADERS)
	$(CC) -o $@ $(CFLAGS) -c $<
contrib/svn-fe/svn-fe: $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $(OBJECTS)
clean:
	$(RM) *.o vcs-svn/*.o contrib/svn-fe/*.o contrib/svn-fe/svn-fe
