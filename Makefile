CFLAGS=
CFLAGS+=-O
#CFLAGS+=-g
#CFLAGS+=-pg
CFLAGS+=-fPIC
LDFLAGS=
#LDFLAGS+=-pg
#VALGRIND=valgrind --leak-check=full

.PHONY: all
all: doc run_examples

.PHONY: help
help:
	@echo "Use one of those prerequisites: run_examples (default), libs, qsip_wc_test, fuzzyword, intensive, timers, test_map, callgraph, cloc, doc or <language>/LC_MESSAGES/libwqm.mo"

#### Examples
.PHONY: run_examples
run_examples: qsip_wc_test fuzzyword intensive timers test_map

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

.PHONY: test_map
test_map: libs examples/timers/test_map
	@echo "********* $@ ************"
	LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:. ./examples/timers/test_map
	@echo "*********************"

examples/qsip/qsip_wc_test: LDFLAGS+=-L.
examples/qsip/qsip_wc_test: LDLIBS=-lwqm -ltimer -lmap
examples/qsip/qsip_wc_test: examples/qsip/qsip_wc_test.o examples/qsip/qsip_wc.o

#examples/qsip/qsip_wc_test.o: CPPFLAGS+=-DSIZE=100 -DTIMES=10    # for valgrind or gdb
examples/qsip/qsip_wc_test.o: CFLAGS+=-std=c23
examples/qsip/qsip_wc_test.o: CPPFLAGS+=-I.

examples/qsip/qsip_wc.o: CFLAGS+=-std=c23
examples/qsip/qsip_wc.o: CPPFLAGS+=-I.

examples/fuzzyword/fuzzyword: CFLAGS+=-std=c23
examples/fuzzyword/fuzzyword: CPPFLAGS+=-DCOLLATE
examples/fuzzyword/fuzzyword: CPPFLAGS+=-I.
examples/fuzzyword/fuzzyword: LDFLAGS+=-L.
examples/fuzzyword/fuzzyword: LDLIBS=-lwqm -ltimer -lmap
examples/fuzzyword/fuzzyword: examples/fuzzyword/fuzzyword.c

examples/intensive/intensive: CFLAGS+=-std=c23
examples/intensive/intensive: CPPFLAGS+=-I.
examples/intensive/intensive: LDFLAGS+=-L.
examples/intensive/intensive: LDLIBS=-lwqm -ltimer -lmap
examples/intensive/intensive: examples/intensive/intensive.c

examples/timers/timers: CFLAGS+=-std=c23
examples/timers/timers: CPPFLAGS+=-I.
examples/timers/timers: LDFLAGS+=-L.
examples/timers/timers: LDLIBS=-lwqm -ltimer -lmap -lrt
examples/timers/timers: examples/timers/timers.c

examples/timers/test_map: CFLAGS+=-std=c23
examples/timers/test_map: CPPFLAGS+=-I.
examples/timers/test_map: LDFLAGS+=-L.
examples/timers/test_map: LDLIBS=-lmap
examples/timers/test_map: examples/timers/test_map.c

#### Tools
.PHONY: callgraph
callgraph:
	@cflow -fposix -n --main threadpool_create_and_start --main threadpool_add_task --main threadpool_cancel_task --main threadpool_wait_and_destroy --main threadpool_task_continuation --main threadpool_task_continue wqm.c | grep -v '<>'

.PHONY: cloc
cloc: map.h map.c timer.h timer.c wqm.h wqm.c
	cloc --quiet --hide-rate --by-file $^

#### Libraries
.PHONY: libs
libs: libmap.a libmap.so libtimer.a libtimer.so libwqm.a libwqm.so

#C11 compliant, since <threads.h> is required.
%.o: CFLAGS+=-std=c11

.INTERMEDIATE: wqm.o
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

#### Documentation
.PHONY: doc

doc: README.html README_map.html README_trace.html README_timer.html

.SECONDARY: README_map.md README_trace.md README_timer.md
README_%.md: %.h ./h2md
	chmod +x ./h2md
	./h2md "$<" >| "$@"

%.html: %.md
	pandoc -f markdown -- "$^" > "$@" || :

