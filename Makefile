CFLAGS+=-O -fPIC

run: libwqm.a libwqm.so qsip_wc_test
	LD_LIBRARY_PATH=$LD_LIBRARY_PATH:. ./qsip_wc_test

qsip_wc_test: LDFLAGS=-L.
qsip_wc_test: LDLIBS=-lwqm
qsip_wc_test: qsip_wc_test.o qsip_wc.o

qsip_wc_test.o: qsip_wc_test.c

qsip_wc.o: qsip_wc.c qsip_wc.h

libwqm.a:

libwqm.so:

wqm.o: wqm.c wqm.h

lib%.so: LDFLAGS+=-shared
lib%.so: %.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^

lib%.a: ARFLAGS=rcs
lib%.a: %.o
	$(AR) $(ARFLAGS) $@ $^
	@nm -A -g --defined-only $@
