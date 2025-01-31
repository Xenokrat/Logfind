CC=clang
CFLAGS=-Wall -Werror -g -fsanitize=address -o3
CPPFLAGS=-DNDEBUG
LDFLAGS=
SOURCES=
OBJECTS=

all: logfind

logfind: logfind.c dbg.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o logfind logfind.c

clean:
	rm -rf logfind

.PHONY: all clean
