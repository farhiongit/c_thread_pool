CFLAGS+=-O

run: libwqm.a qsip_wc_test
	./qsip_wc_test

qsip_wc_test: LDFLAGS=-L.
qsip_wc_test: LDLIBS=-lwqm
qsip_wc_test: qsip_wc_test.o qsip_wc.o

qsip_wc_test.o: qsip_wc_test.c

qsip_wc.o: qsip_wc.c qsip_wc.h

libwqm.a:

wqm.o: wqm.c wqm.h

lib%.a: ARFLAGS=rcs
lib%.a: %.o
	$(AR) $(ARFLAGS) $@ $^
	@nm -A -g --defined-only $@
