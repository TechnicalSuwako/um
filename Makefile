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

.if ${UNAME_M} == "X86_64"
ARCH = amd64
.endif

NAME != cat main.c | grep "const char \*sofname" | awk '{print $$5}' |\
	sed "s/\"//g" | sed "s/;//"
VERSION != cat main.c | grep "const char \*version" | awk '{print $$5}' |\
	sed "s/\"//g" | sed "s/;//"

PREFIX = /usr/local
.if ${OS} == "linux"
PREFIX = /usr
.endif

CC = cc
FILES = main.c

CFLAGS = -Wall -Wextra -I/usr/include -L/usr/lib
.if ${OS} == "netbsd"
CFLAGS += -I/usr/X11R7/include -I/usr/X11R7/include/freetype2 -L/usr/X11R7/lib
.elif ${OS} == "openbsd"
CFLAGS += -I/usr/X11R6/include -I/usr/X11R6/include/freetype2 -L/usr/X11R6/lib
.elif ${OS} == "freebsd"
CFLAGS += -I/usr/local/include -I/usr/local/include/X11\
					-I /usr/local/include/freetype2\
					-L/usr/local/lib
.elif ${OS} == "linux"
CFLAGS += -I/usr/include/freetype2
.endif

LDFLAGS = -lc -lX11 -lXft
SLIB = -lxcb
.if ${OS} == "openbsd"
SLIB += -lfontconfig -lz -lexpat -lfreetype -lXrender -lXau -lXdmcp
.elif ${OS} == "freebsd"
SLIB += -lthr -lfontconfig -lfreetype -lXrender -lXau -lXdmcp -lexpat -lz -lbz2\
				-lpng16 -lbrotlidec -lm -lbrotlicommon
.elif ${OS} == "netbsd"
SLIB += -lfontconfig -lfreetype -lXau -lXdmcp -lgcc -lexpat -lz -lbz2 -lXrandr\
				-lXrender -lXext -lX11
.elif ${OS} == "linux"
SLIB += -lfontconfig -lfreetype -lXrender -lXau -lXdmcp -lX11\
				-lexpat -lpng16 -lbz2 -lz\
				-lbrotlidec -lbrotlicommon
.endif

all:
	${CC} -O3 ${CFLAGS} -o ${NAME} ${FILES} -static ${LDFLAGS} ${SLIB}
	strip ${NAME}

debug:
	${CC} -g ${CFLAGS} -o ${NAME} ${FILES} ${LDFLAGS}

clean:
	rm -rf ${NAME}

dist:
	mkdir -p ${NAME}-${VERSION} release/src
	cp -R LICENSE.txt Makefile README.md CHANGELOG.md\
		main.c ${NAME}-${VERSION}
	tar zcfv release/src/${NAME}-${VERSION}.tar.gz ${NAME}-${VERSION}
	rm -rf ${NAME}-${VERSION}

release:
	mkdir -p release/bin/${VERSION}/${OS}/${ARCH}
	${CC} -O3 ${CFLAGS} -o release/bin/${VERSION}/${OS}/${ARCH}/${NAME} ${FILES}\
		-static ${LDFLAGS} ${SLIB}
	strip release/bin/${VERSION}/${OS}/${ARCH}/${NAME}

install:
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${NAME} ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/${NAME}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${NAME}

.PHONY: all debug clean dist release install uninstall
