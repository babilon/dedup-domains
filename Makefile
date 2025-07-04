CC := gcc
SRCDIR := src
OBJDIR := obj
BINDIR := bin
DIRMAIN := $(OBJDIR)/main
DIRREL := $(OBJDIR)/release
DIRTEST := $(OBJDIR)/test
DIRCODECOV := $(OBJDIR)/codecov
DIRTEST_SIZE_T := $(OBJDIR)/test_size_t

INCLUDE := -Iinclude/ -Igenerated/include/

MEMSET := -DUSE_MEMSET
SIZE_T := -DUSE_SIZE_T
DIAGNO := -DCOLLECT_DIAGNOSTICS
CODECOV := -fPIC -fprofile-arcs -ftest-coverage

DEBUGFLAG := -g3
TESTFLAGS := $(DEBUGFLAG) -DBUILD_TESTS $(SIZE_T) $(MEMSET)
CODECOVFLAGS := $(DEBUGFLAG) $(TESTFLAGS) $(CODECOV)
MAINFLAGS := $(DEBUGFLAG) $(DIAGNO)
RELFLAGS := -O3 -DRELEASE_LOGGING -DCOLLECT_DIAGNOSTICS -DNDEBUG

CFLAGS := -std=c17 -Wall -Wextra -Werror
LFLAGS := -std=c17 -Wall -Wextra -Werror

VERSIONDOTH := include/version.h
SRC ?=
SRCTEST ?= $(SRC)

include src/Submakefile src/tests/Submakefile

OBJREL := $(patsubst %.c,$(DIRREL)/%.o,$(SRC))
OBJMAIN := $(patsubst %.c,$(DIRMAIN)/%.o,$(SRC))
OBJTEST := $(patsubst %.c,$(DIRTEST)/%.o,$(SRCTEST))
OBJCODECOV := $(patsubst %.c,$(DIRCODECOV)/%.o,$(SRCTEST))
VERSIONNOGIT := $(patsubst %.h,generated/%.nogit.h,$(VERSIONDOTH))

generated/%.nogit.h: $(VERSIONDOTH) createversion.sh
	/bin/sh ./createversion.sh

$(DIRMAIN)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo Using $(CC) to compile $< for main debug..
	@$(CC) -c $(INCLUDE) -o $@ $< $(CFLAGS) $(MAINFLAGS)

$(DIRTEST)/%.o: %.c
	@echo $@
	@mkdir -p $(dir $@)
	@echo Compiling $< for unit testing..
	@$(CC) -c $(INCLUDE) -o $@ $< $(CFLAGS) $(TESTFLAGS)

$(DIRCODECOV)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo Compiling $< for code coverage..
	@$(CC) -c $(INCLUDE) -o $@ $< $(CFLAGS) $(CODECOVFLAGS)

$(DIRREL)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo Compiling $< for release..
	@$(CC) -c $(INCLUDE) -o $@ $< $(CFLAGS) $(RELFLAGS)

.PHONY: all
all: main release test codecoverage

main: $(VERSIONNOGIT) $(OBJMAIN)
	@echo Linking $@
	@mkdir -p ${BINDIR}
	@$(CC) $(LFLAGS) $(OBJMAIN) -o ./${BINDIR}/$@.real

release: $(VERSIONNOGIT) $(OBJREL)
	@echo Linking $@
	@mkdir -p ${BINDIR}
	@$(CC) $(FLAGS) $(OBJREL) -o ./${BINDIR}/$@.real

fpos: obj/testfpos.o
	@mkdir -p ${BINDIR}
	@$(CC) $(LFLAGS) $^ -o ./${BINDIR}/$@.real

test: $(VERSIONNOGIT) $(OBJTEST)
	@mkdir -p ${BINDIR}
	@$(CC) $(CFLAGS) $(TESTFLAGS) $(OBJTEST) -o ./${BINDIR}/$@.real

testsizet: $(VERSIONNOGIT) $(OBJTEST_SIZE_T)
	@mkdir -p ${BINDIR}
	@$(CC) $(CFLAGS) $(TESTFLAGS) $(SIZE_T) $(OBJTEST_SIZE_T) -o ./${BINDIR}/$@.real

codecoverage: $(VERSIONNOGIT) $(OBJCODECOV)
	@mkdir -p ${BINDIR}
	@$(CC) $(CFLAGS) $(CODECOVFLAGS) $(OBJCODECOV) -o ./${BINDIR}/$@.real

.PHONY: clean
clean:
	@rm -rf ./generated/
	@rm -rf ./$(OBJDIR)
	@rm -rf ./${BINDIR}
