CC ?= gcc
STRIP ?= strip
CFLAGS = -std=c99 -pedantic -Wall
ifdef DEBUG
  CFLAGS += -g3
else
  CFLAGS += -Os -DNDEBUG
endif
LDFLAGS = -lxcb

BIN = pixbar
SRCS = pixbar.c
OBJS = ${SRCS:.c=.o}

PREFIX?=/usr
BINDIR=${PREFIX}/bin

.PHONY: all
all: ${BIN}

.c.o:
	${CC} ${CFLAGS} -o $@ -c $<

${BIN}: ${OBJS}
	${CC} -o $@ $^ ${LDFLAGS}
	${STRIP} -s $@

.PHONY: clean
clean:
	rm -f ./*.o
