CFLAGS+=-O
#CFLAGS+=-g
#CFLAGS+=-pg
CFLAGS+=-fPIC
LDFLAGS=
#LDFLAGS+=-pg
#CHECK=valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes
#CHECK=gdb

.PHONY: all
all: run_examples

.PHONY: help
help:
	@echo "Use one of those prerequisites: run_examples (default), libs, qsip_wc_test, fuzzyword, intensive, timers, mfr, callgraph, cloc or <language>/LC_MESSAGES/libwqm.mo"

#### Examples
.PHONY: run_examples
run_examples: qsip_wc_test fuzzyword intensive timers mfr

.PHONY: qsip_wc_test
qsip_wc_test: libs examples/qsip/qsip_wc_test
	@echo "********* $@ ************"
	cd examples/qsip ; LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:../..:../../../minimaps ./qsip_wc_test
	@echo "*********************"

.PHONY: fuzzyword
fuzzyword: libs examples/fuzzyword/fuzzyword
	@echo "********* $@ ************"
	cd examples/fuzzyword ; LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:../..:../../../minimaps ./fuzzyword \
	premir ministr constitutiotn arlmée républiqeu plaine pouvoit résrevé apanache finances dépenser contgaint paliatiff constitutionnaliseraint
	@echo "*********************"

.PHONY: intensive
intensive: libs examples/intensive/intensive
	@echo "********* $@ ************"
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:.:../minimaps ./examples/intensive/intensive
	@echo "*********************"

.PHONY: timers
timers: libs examples/continuations/timers
	@echo "********* $@ ************"
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:.:../minimaps $(CHECK) ./examples/continuations/timers
	@echo "*********************"

.PHONY: mfr
mfr: libs examples/mfr/mfr
	@echo "********* $@ ************"
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:.:../minimaps $(CHECK) ./examples/mfr/mfr
	@echo "*********************"

examples/qsip/qsip_wc_test: LDFLAGS+=-L. -L../minimaps
examples/qsip/qsip_wc_test: LDLIBS=-lwqm -ltimer -lmap
examples/qsip/qsip_wc_test: examples/qsip/qsip_wc_test.o examples/qsip/qsip_wc.o

#examples/qsip/qsip_wc_test.o: CPPFLAGS+=-DSIZE=100 -DTIMES=10    # for valgrind or gdb
.INTERMEDIATE: examples/qsip/qsip_wc_test.o
examples/qsip/qsip_wc_test.o: CFLAGS+=-std=c23
examples/qsip/qsip_wc_test.o: CPPFLAGS+=-I. -I../minimaps

.INTERMEDIATE: examples/qsip/qsip_wc.o
examples/qsip/qsip_wc.o: CFLAGS+=-std=c23
examples/qsip/qsip_wc.o: CPPFLAGS+=-I. -I../minimaps

examples/fuzzyword/fuzzyword: CFLAGS+=-std=c23
examples/fuzzyword/fuzzyword: CPPFLAGS+=-DCOLLATE
examples/fuzzyword/fuzzyword: CPPFLAGS+=-I. -I../minimaps
examples/fuzzyword/fuzzyword: LDFLAGS+=-L. -L../minimaps
examples/fuzzyword/fuzzyword: LDLIBS=-lwqm -ltimer -lmap

examples/intensive/intensive: CFLAGS+=-std=c23
examples/intensive/intensive: CPPFLAGS+=-I. -I../minimaps
examples/intensive/intensive: LDFLAGS+=-L. -L../minimaps
examples/intensive/intensive: LDLIBS=-lwqm -ltimer -lmap

examples/continuations/timers: CFLAGS+=-std=c23
examples/continuations/timers: CPPFLAGS+=-I. -I../minimaps
examples/continuations/timers: LDFLAGS+=-L. -L../minimaps
examples/continuations/timers: LDLIBS=-lwqm -ltimer -lmap

examples/mfr/mfr: CPPFLAGS+=-I. -I../minimaps
examples/mfr/mfr: LDFLAGS+=-L. -L../minimaps
examples/mfr/mfr: LDLIBS+=-lwqm -ltimer -lmap

#### Tools
.PHONY: callgraph
callgraph:
	@cflow -fposix -n --main threadpool_create_and_start --main threadpool_add_task --main threadpool_cancel_task --main threadpool_wait_and_destroy --main threadpool_task_continuation --main threadpool_task_continue wqm.c | grep -v '<>'

.PHONY: cloc
cloc: wqm.h wqm.c
	cloc --quiet --hide-rate --by-file $^

#### Libraries
.PHONY: libs
libs: libwqm.a libwqm.so

#C11 compliant, since <threads.h> is required.
%.o: CFLAGS+=-std=c11

.INTERMEDIATE: wqm.o
wqm.o: CPPFLAGS+=-I. -I../minimaps
# Uncomment to prepare for gettext
#wqm.o: CPPFLAGS+=-I/usr/share/gettext -include gettext.h -DENABLE_NLS=1
#wqm.o: CPPFLAGS+=-DPACKAGE="\"libwqm\"" -DLOCALEDIR="\"${PWD}\"" -D"_(s)"="dgettext(PACKAGE,s)" -Di18n_init="do{bindtextdomain(PACKAGE,LOCALEDIR);}while(0)"

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

