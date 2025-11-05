NAME = dwmstatus
VERSION = 1.0

# paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS = -I. -I/usr/include
LIBS = -L/usr/lib -lc $(shell pkg-config --libs libuv alsa libudev x11)

# extra source files
SRCS = batudev.c backend.c alsamixer.c

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\" -D_DEFAULT_SOURCE
CFLAGS = -g -std=c11 -pedantic -Wall -O2 ${INCS} ${CPPFLAGS}
LDFLAGS = -g ${LIBS}

# compiler and linker
CC = clang

