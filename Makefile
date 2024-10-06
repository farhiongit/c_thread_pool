CFLAGS+=-O

run: qsip_wc_test info
	./qsip_wc_test

qsip_wc_test: qsip_wc_test.o qsip_wc.o wqm.o

qsip_wc_test.o: qsip_wc_test.c qsip_wc.o wqm.o

qsip_wc.o: qsip_wc.c qsip_wc.h

wqm.o: wqm.c wqm.h

.PHONY: info
info: wqm.o qsip_wc.o
	@nm -A -g --defined-only $?
