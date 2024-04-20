CFLAGS+=-Wall
CC=clang
#CFLAGS+=-g
#CFLAGS+=-DDEBUG
CFLAGS+=-O3

all: qsip_wc_test info

#qsip_wc_test: CFLAGS += -DSIZE=100 -DTIMES=10
#qsip_wc_test: CFLAGS += -DQSORT
#qsip_wc_test: CFLAGS+=-DTIMES=20
qsip_wc_test: qsip_wc_test.c qsip_wc.o wqm.o

#qsip_wc.o: CFLAGS += -DFIXED_PIVOT
qsip_wc.o: qsip_wc.c qsip_wc.h wqm.o

wqm.o: wqm.c wqm.h

wqm_tu: LDLIBS += -lcheck -lm -lsubunit
wqm_tu: LDFLAGS +=
wqm_tu: wqm.o

.PHONY: info
info: wqm.o qsip_wc.o
	@nm -A -g --defined-only $?
