# tsdp Makefile
#
# author:  James Hunt <james@niftylogic.com>
# created: 2016-07-12
#

CC       ?= clang
AFLCC    ?= afl-clang
COVER    ?= llvm-cov
TABLEGEN := util/tablegen

CPPFLAGS += -I./include -I./src

default: all

# files to remove on `make clean'
CLEAN_FILES :=

# header files required by all object files.
CORE_H := include/tsdp.h

# header files that should be distributed.
DIST_H := include/tsdp.h


# source files that comprise the Qualified Name implementation.
QNAME_SRC  := src/qname.c
QNAME_OBJ  := $(QNAME_SRC:.c=.o)
QNAME_SO   := $(QNAME_SRC:.c=.lib.o)
QNAME_FUZZ := $(QNAME_SRC:.c=.fuzz.o)
QNAME_COV  := $(QNAME_SRC:.c=.cov.o)
CLEAN_FILES += $(QNAME_OBJ) $(QNAME_SO) $(QNAME_FUZZ) $(QNAME_COV)

src/qname_chars.inc: src/qname_chars.tbl $(TABLEGEN)
	$(TABLEGEN) >$@ <$<
src/qname.o: src/qname.c $(CORE_H) src/qname_chars.inc
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

# source files that comprise the Message implementation.
MSG_SRC  := src/msg.c
MSG_OBJ  := $(MSG_SRC:.c=.o)
MSG_LO   := $(MSG_SRC:.c=.lib.o)
MSG_FUZZ := $(MSG_SRC:.c=.fuzz.o)
MSG_COV  := $(MSG_SRC:.c=.cov.o)
CLEAN_FILES += $(MSG_OBJ) $(MSG_SO) $(MSG_FUZZ) $(MSG_COV)


# scripts that perform Contract Testing.
CONTRACT_TEST_SCRIPTS := t/contract/qname \
                         t/contract/msg

# binaries that the Contract Tests run.
CONTRACT_TEST_BINS := t/contract/r/qname-base \
                      t/contract/r/qname-string \
                      t/contract/r/qname-equiv \
                      t/contract/r/qname-match \
                      t/contract/r/qname-max \
                      t/contract/r/msg-acc \
                      t/contract/r/msg-in \
                      t/contract/r/msg-out
CLEAN_FILES   += $(CONTRACT_TEST_BINS)
CLEAN_FILES   += $(CONTRACT_TEST_BINS:=.o)

contract-tests: $(CONTRACT_TEST_BINS)
t/contract/r/qname-base: t/contract/r/qname-base.o $(QNAME_COV)
	$(CC) $(LDFLAGS) --coverage $+ -o $@
t/contract/r/qname-string: t/contract/r/qname-string.o $(QNAME_COV)
	$(CC) $(LDFLAGS) --coverage $+ -o $@
t/contract/r/qname-equiv: t/contract/r/qname-equiv.o $(QNAME_COV)
	$(CC) $(LDFLAGS) --coverage $+ -o $@
t/contract/r/qname-match: t/contract/r/qname-match.o $(QNAME_COV)
	$(CC) $(LDFLAGS) --coverage $+ -o $@
t/contract/r/qname-max: t/contract/r/qname-max.o $(QNAME_COV)
	$(CC) $(LDFLAGS) --coverage $+ -o $@
t/contract/r/msg-acc: t/contract/r/msg-acc.o $(MSG_COV)
	$(CC) $(LDFLAGS) --coverage $+ -o $@
t/contract/r/msg-in: t/contract/r/msg-in.o $(MSG_COV)
	$(CC) $(LDFLAGS) --coverage $+ -o $@
t/contract/r/msg-out: t/contract/r/msg-out.o $(MSG_COV)
	$(CC) $(LDFLAGS) --coverage $+ -o $@

check-contract: $(CONTRACT_TEST_BINS)
	for test in $(CONTRACT_TEST_SCRIPTS); do echo $$test; $$test || exit $$?; echo; done


# scripts that perform Memory Testing.
MEM_TEST_SCRIPTS := t/mem/malloc

# binaries that the Memory Tests run.
MEM_TEST_BINS := t/mem/r/qname
CLEAN_FILES += $(MEM_TEST_BINS)
CLEAN_FILES += $(MEM_TEST_BINS:=.o)

mem-tests: $(MEM_TEST_BINS)
t/mem/r/qname: t/mem/r/qname.o $(QNAME_OBJ)
	$(CC) $(LDFLAGS) $+ -o $@

check-mem: $(MEM_TEST_BINS)
	for test in $(MEM_TEST_SCRIPTS); do echo $$test; $$test || exit $$?; echo; done


# binaries that the Fuzz Tests run.
FUZZ_TEST_BINS := t/fuzz/r/qname \
                  t/fuzz/r/msg
CLEAN_FILES   += $(FUZZ_TEST_BINS)
CLEAN_FILES   += $(FUZZ_TEST_BINS:=.o)

fuzz-tests: $(FUZZ_TEST_BINS)
t/fuzz/r/qname:  t/fuzz/r/qname.o  $(QNAME_FUZZ)
t/fuzz/r/msg:    t/fuzz/r/msg.o    $(MSG_FUZZ)

%.fuzz.o: %.c
	$(AFLCC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

%.cov.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fprofile-arcs -ftest-coverage -c -o $@ $<

%.lib.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c -o $@ $<


clean:
	rm -fr $(CLEAN_FILES)
	rm -fr $(CLEAN_FILES:.o=.gcda)
	rm -fr $(CLEAN_FILES:.o=.gcno)

libs: libtsdp.a libtsdp.so
# static library
libtsdp.a: $(QNAME_OBJ) $(MSG_OBJ)
	ar cr $@ $+
# dynamic library
libtsdp.so: $(QNAME_LO) $(MSG_LO)
	$(CC) -shared -o $@ $+

all: test libs

check: check-contract
test: check
cover: check coverage
coverage:
	lcov --directory . \
	     --base-directory . \
	     --gcov-tool ./util/llvm-gcov \
	     --capture -o lcov.info
	genhtml -o coverage lcov.info
CLEAN_FILES += coverage
CLEAN_FILES += lcov.info


.PHONY: all clean test check check-mem coverage
