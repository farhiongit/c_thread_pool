CFLAGS=
CFLAGS+=-O
#CFLAGS+=-ggdb
CFLAGS+=-fPIC
#VALGRIND=valgrind --leak-check=full

.PHONY: run_examples
run_examples: qsip_wc_test fuzzyword intensive timers

.PHONY: help
help:
	@echo "Use one of those prerequisites: run_examples (default), libs, qsip_wc_test, fuzzyword, intensive, timers, callgraph, README.html, <language>/LC_MESSAGES/libwqm.mo"

#### Examples
.PHONY: qsip_wc_test
qsip_wc_test: libs examples/qsip/qsip_wc_test
	@echo "********* $@ ************"
	cd examples/qsip ; LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:../.. $(VALGRIND) ./qsip_wc_test
	@echo "*********************"

.PHONY: fuzzyword
fuzzyword: libs examples/fuzzyword/fuzzyword
	@echo "********* $@ ************"
	cd examples/fuzzyword ; LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:../.. ./fuzzyword \
	premir ministr constitutiotn arlmée républiqeu plaine pouvoit résrevé apanache finances dépenser contgaint paliatiff constitutionnaliseraint
	@echo "*********************"

.PHONY: intensive
intensive: libs examples/intensive/intensive
	@echo "********* $@ ************"
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./examples/intensive/intensive
	@echo "*********************"

.PHONY: timers
timers: libs examples/timers/timers
	@echo "********* $@ ************"
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./examples/timers/timers
	@echo "*********************"

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

examples/timers/timers: CPPFLAGS+=-I.
examples/timers/timers: LDFLAGS=-L.
examples/timers/timers: LDLIBS=-lwqm -lm -lrt
examples/timers/timers: examples/timers/timers.c

#### Libraries
.PHONY: callgraph
callgraph:
	@cflow -fposix -n --main threadpool_create_and_start --main threadpool_add_task --main threadpool_cancel_task --main threadpool_wait_and_destroy --main threadpool_task_continuation --main threadpool_task_continue wqm.c | grep -v '<>'

.PHONY: libs
libs: libwqm.a libwqm.so

libwqm.a:

libwqm.so:

wqm.o: CFLAGS+=-std=c11  #C11 compliant
# Uncomment to prepare for gettext
#wqm.o: CPPFLAGS+=-I/usr/share/gettext -include gettext.h -DENABLE_NLS=1
#wqm.o: CPPFLAGS+=-DPACKAGE="\"libwqm\"" -DLOCALEDIR="\"${PWD}\"" -D"_(s)"="dgettext(PACKAGE,s)" -Di18n_init="do{bindtextdomain(PACKAGE,LOCALEDIR);}while(0)"
wqm.o: wqm.c wqm.h

lib%.so: LDFLAGS+=-shared
lib%.so: %.o
	$(CC) $(LDFLAGS) -o "$@" "$^"

lib%.a: ARFLAGS=rcs
lib%.a: %.o
	rm -f -- "$@"
	$(AR) $(ARFLAGS) -- "$@" "$^"
	@nm -A -g --defined-only -- "$@"

#### Internationalization
# Prepare for gettext (with make fr/LC_MESSAGES/libwqm.mo for instance)
%/LC_MESSAGES/libwqm.mo: po/%.po
	mkdir -p "$(dir $@)"
	msgfmt --output-file="$@" -- "$^"

po/fr.po: po/libwqm.pot
	mkdir -p "$(dir $@)"
	[ ! -f "$@" ] || msgmerge -U -N --lang=fr -i --no-location --no-wrap -- "$@" "$^"
	[ -f "$@" ] || msginit --no-translator -l fr --no-wrap -i "$^" -o "$@"

po/libwqm.pot: wqm.c
	mkdir -p "$(dir $@)"
	xgettext -o "$@" -LC -k_ -i --package-name=libwqm --no-wrap --no-location -- "$^"

#### README to html
README.html: README.md
	pandoc -f markdown -- "$^" > "$@"

