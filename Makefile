.PHONY: all clean
CFLAGS = -Wall -W -g -O2
HEADERS = compat-util.h fast_export.h line_buffer.h mkgmtime.h \
	obj_pool.h repo_tree.h sliding_window.h strbuf.h \
	string_pool.h svndiff.h svndump.h trp.h
OBJECTS = fast_export.o line_buffer.o mkgmtime.o repo_tree.o \
	sliding_window.o strbuf.o string_pool.o svndiff.o svndump.o \
	svn-fe.o

all: svn-fe
%.o: %.c $(HEADERS)
	$(CC) -o $@ $(CFLAGS) -c $<
svn-fe: $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $(OBJECTS)
clean:
	$(RM) *.o svn-fe
