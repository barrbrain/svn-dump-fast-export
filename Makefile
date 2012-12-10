.PHONY: all clean
CFLAGS = -Wall -W -g -O2 -Icompat -Ivcs-svn
LIBGIT2_PATH ?= $(HOME)/dev/libgit2
CFLAGS += -I$(LIBGIT2_PATH)/include
LDFLAGS += -L$(LIBGIT2_PATH) -lgit2

HEADERS = compat/mkgmtime.h \
	compat/quote.h \
	compat/strbuf.h \
	vcs-svn/compat-util.h \
	vcs-svn/fast_export.h \
	vcs-svn/line_buffer.h \
	vcs-svn/repo_tree.h \
	vcs-svn/sliding_window.h \
	vcs-svn/svndiff.h \
	vcs-svn/svndump.h

OBJECTS = compat/mkgmtime.o \
	compat/quote.o \
	compat/strbuf.o \
	contrib/svn-fe/svn-fe.o \
	vcs-svn/fast_export.o \
	vcs-svn/line_buffer.o \
	vcs-svn/repo_tree.o \
	vcs-svn/sliding_window.o \
	vcs-svn/svndiff.o \
	vcs-svn/svndump.o

all: contrib/svn-fe/svn-fe
%.o: %.c $(HEADERS)
	$(CC) -o $@ $(CFLAGS) -c $<
contrib/svn-fe/svn-fe: $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJECTS)
clean:
	$(RM) compat/*.o vcs-svn/*.o \
	contrib/svn-fe/*.o contrib/svn-fe/svn-fe
