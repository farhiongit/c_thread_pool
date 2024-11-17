CFLAGS+=-O
CFLAGS+=-fPIC

.PHONY: all
all: qsip_wc_test fuzzyword

.PHONY: qsip_wc_test
qsip_wc_test: libs examples/qsip/qsip_wc_test
	cd examples/qsip ; LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../.. ./qsip_wc_test

.PHONY: fuzzyword
fuzzyword: libs examples/fuzzyword/fuzzyword
	cd examples/fuzzyword ; LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../.. ./fuzzyword \
	premir ministr constitutiotn arlmée républiqeu plaine pouvoit résrevé apanache finances dépenser contgaint paliatiff constitutionnaliseraint

examples/qsip/qsip_wc_test: LDFLAGS=-L.
examples/qsip/qsip_wc_test: LDLIBS=-lwqm -lm
examples/qsip/qsip_wc_test: examples/qsip/qsip_wc_test.o examples/qsip/qsip_wc.o

examples/qsip/qsip_wc_test.o: CFLAGS+=-I.
examples/qsip/qsip_wc_test.o: examples/qsip/qsip_wc_test.c

examples/qsip/qsip_wc.o: CFLAGS+=-I.
examples/qsip/qsip_wc.o: examples/qsip/qsip_wc.c examples/qsip/qsip_wc.h

examples/fuzzyword/fuzzyword: CFLAGS+=-DCOLLATE
examples/fuzzyword/fuzzyword: CFLAGS+=-I.
examples/fuzzyword/fuzzyword: LDFLAGS=-L.
examples/fuzzyword/fuzzyword: LDLIBS=-lwqm -lm
examples/fuzzyword/fuzzyword: examples/fuzzyword/fuzzyword.c

.PHONY: libs
libs: libwqm.a libwqm.so

libwqm.a:

libwqm.so:

wqm.o: CFLAGS+=-std=c11
wqm.o: wqm.c wqm.h

lib%.so: LDFLAGS+=-shared
lib%.so: %.o
	$(CC) $(LDFLAGS) -o $@ $^

lib%.a: ARFLAGS=rcs
lib%.a: %.o
	$(AR) $(ARFLAGS) $@ $^
	@nm -A -g --defined-only $@
