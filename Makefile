CFLAGS+=-O
CFLAGS+=-fPIC

.PHONY: all
all: qsip_wc_test fuzzyword intensive

.PHONY: qsip_wc_test
qsip_wc_test: libs examples/qsip/qsip_wc_test
	cd examples/qsip ; LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:../.. ./qsip_wc_test

.PHONY: fuzzyword
fuzzyword: libs examples/fuzzyword/fuzzyword
	cd examples/fuzzyword ; LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:../.. ./fuzzyword \
	premir ministr constitutiotn arlmée républiqeu plaine pouvoit résrevé apanache finances dépenser contgaint paliatiff constitutionnaliseraint

.PHONY: intensive
intensive: libs examples/intensive/intensive
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./examples/intensive/intensive

examples/qsip/qsip_wc_test: LDFLAGS=-L.
examples/qsip/qsip_wc_test: LDLIBS=-lwqm -lm
examples/qsip/qsip_wc_test: examples/qsip/qsip_wc_test.o examples/qsip/qsip_wc.o

#examples/qsip/qsip_wc_test.o: CPPFLAGS+=-DSIZE=100 -DTIMES=10    # for valgrind or gdb
examples/qsip/qsip_wc_test.o: CPPFLAGS+=-I.
examples/qsip/qsip_wc_test.o: examples/qsip/qsip_wc_test.c

examples/qsip/qsip_wc.o: CPPFLAGS+=-I.
examples/qsip/qsip_wc.o: examples/qsip/qsip_wc.c examples/qsip/qsip_wc.h

examples/fuzzyword/fuzzyword: CPPFLAGS+=-DCOLLATE
examples/fuzzyword/fuzzyword: CPPFLAGS+=-I.
examples/fuzzyword/fuzzyword: LDFLAGS=-L.
examples/fuzzyword/fuzzyword: LDLIBS=-lwqm -lm
examples/fuzzyword/fuzzyword: examples/fuzzyword/fuzzyword.c

examples/intensive/intensive: CPPFLAGS+=-I.
examples/intensive/intensive: LDFLAGS=-L.
examples/intensive/intensive: LDLIBS=-lwqm -lm
examples/intensive/intensive: examples/intensive/intensive.c

.PHONY: libs
libs: libwqm.a libwqm.so

libwqm.a:

libwqm.so:

wqm.o: CFLAGS+=-std=c11  #C11 compliant
# Prepare for gettext (uncomment)
#wqm.o: CPPFLAGS+=-I/usr/share/gettext -include gettext.h -DENABLE_NLS=1
#wqm.o: CPPFLAGS+=-DPACKAGE="\"libwqm\"" -DLOCALEDIR="\"${PWD}\"" -D"_(s)"="dgettext(PACKAGE,s)" -Di18n_init="do{bindtextdomain(PACKAGE,LOCALEDIR);}while(0)"
wqm.o: wqm.c wqm.h

lib%.so: LDFLAGS+=-shared
lib%.so: %.o
	$(CC) $(LDFLAGS) -o $@ $^

lib%.a: ARFLAGS=rcs
lib%.a: %.o
	$(AR) $(ARFLAGS) $@ $^
	@nm -A -g --defined-only $@

# Prepare for gettext
%/LC_MESSAGES/libwqm.mo: po/%.po
	mkdir -p "$(dir $@)"
	msgfmt --output-file=$@ $^

po/fr.po: po/libwqm.pot
	mkdir -p "$(dir $@)"
	[ ! -f $@ ] || msgmerge -U -N --lang=fr -i --no-location --no-wrap $@ $^
	[ -f $@ ] || msginit --no-translator -l fr --no-wrap -i $^ -o $@

po/libwqm.pot: wqm.c
	mkdir -p "$(dir $@)"
	xgettext -o $@ -LC -k_ -i --package-name=libwqm --no-wrap --no-location $^

README.html: README.md
	pandoc -f markdown $^ > $@

