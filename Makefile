UNAME_S != uname -s
UNAME_M != uname -m
OS = ${UNAME_S}
ARCH = ${UNAME_M}

.if ${UNAME_S} == "OpenBSD"
OS = openbsd
.elif ${UNAME_S} == "NetBSD"
OS = netbsd
.elif ${UNAME_S} == "FreeBSD"
OS = freebsd
.elif ${UNAME_S} == "Dragonfly"
OS = dragonfly
.elif ${UNAME_S} == "Linux"
OS = linux
.endif

.if ${UNAME_M} == "X86_64
ARCH = amd64
.endif

NAME = um
VERSION = 0.0.0

PREFIX = /usr/local
.if ${OS} == "linux"
PREFIX = /usr
.endif

MANPREFIX = ${PREFIX}/share/man
.if ${OS} == "openbsd"
MANPREFIX = ${PREFIX}/man
.endif

CC = cc
FILES = main.c

CFLAGS = -Wall -Wextra -O3 -I/usr/include -L/usr/lib
.if ${OS} == "netbsd"
CFLAGS += -I/usr/X11R7/include -L/usr/X11R7/lib
.elif ${OS} == "openbsd"
CFLAGS += -I/usr/X11R6/include -I/usr/X11R6/include/freetype2 -L/usr/X11R6/lib
.elif ${OS} == "freebsd"
CFLAGS += -I/usr/local/include -L/usr/local/lib
.endif

LDFLAGS = -lc -lX11 -lXft
SLIB = -lxcb -lfontconfig -lz -lexpat -lfreetype -lXrender -lXau -lXdmcp

all:
	${CC} ${CFLAGS} -o ${NAME} ${FILES} -static ${LDFLAGS} ${SLIB}
	strip ${NAME}

debug:
	${CC} -Wall -Wextra -g -I/usr/include -L/usr/lib \
		-I/usr/X11R6/include -I/usr/X11R6/include/freetype2 -L/usr/X11R6/lib \
		-o ${NAME} ${FILES} ${LDFLAGS}

clean:
	rm -rf ${NAME}

.PHONY: all debug clean
