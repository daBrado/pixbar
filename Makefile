CC ?= gcc
STRIP ?= strip
CFLAGS = -std=gnu99 -pedantic -Wall
ifdef DEBUG
  CFLAGS += -g3
else
  CFLAGS += -Os -DNDEBUG
endif
LDFLAGS = -lxcb -lxcb-image

PREFIX ?= /usr
BINDIR = ${PREFIX}/bin

BIN = pixbar
SRCS = pixbar.c
OBJS = ${SRCS:.c=.o}
ALLBIN = ${BIN} pixbard pixbarc

.PHONY: all
all: ${BIN}

.c.o:
	${CC} ${CFLAGS} -o $@ -c $<

${BIN}: ${OBJS}
	${CC} -o $@ $^ ${LDFLAGS}
	${STRIP} -s $@

.PHONY: install
install:
	mkdir -p ${BINDIR}
	cp -f ${ALLBIN} ${BINDIR}

.PHONY: clean
clean:
	rm -f ./*.o
