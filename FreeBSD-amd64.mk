.DEFAULT_GOAL := main

CC := clang
SRCDIR := src
OBJDIR := obj
BINDIR := bin
DIRMAIN := $(OBJDIR)/main
DIRREGEX := $(OBJDIR)/regex
DIRREL := $(OBJDIR)/release
DIRTEST := $(OBJDIR)/test
DIRCODECOV := $(OBJDIR)/codecov
DIRTEST_SIZE_T := $(OBJDIR)/test_size_t

INCLUDE := include/

MEMSET := -DUSE_MEMSET
SIZE_T := -DUSE_SIZE_T
DIAGNO := -DCOLLECT_DIAGNOSTICS
CODECOV := -DCODECOVERAGE

DEBUGFLAG := -g3
TESTFLAGS := $(DEBUGFLAG) -DBUILD_TESTS $(SIZE_T) $(MEMSET)
CODECOVFLAGS := $(DEBUGFLAG) $(TESTFLAGS) $(CODECOV)
MAINFLAGS := $(DEBUGFLAG) $(DIAGNO)
REGEXFLAGS := $(DEBUGFLAG) $(DIAGNO) -DREGEX_ENABLED
RELFLAGS := -O3 -DRELEASE_LOGGING -DNDEBUG

CFLAGS := -std=c17 -Wall -Wextra -Werror -I$(INCLUDE)
LFLAGS := -std=c17 -Wall -Wextra -Werror

SRC ?= 
SRCTEST ?= $(SRC)

include src/Submakefile src/tests/Submakefile

OBJREL := $(patsubst %.c,$(DIRREL)/%.o,$(SRC))
OBJMAIN := $(patsubst %.c,$(DIRMAIN)/%.o,$(SRC))
OBJREGEX := $(patsubst %.c,$(DIRREGEX)/%.o,$(SRC))
OBJTEST := $(patsubst %.c,$(DIRTEST)/%.o,$(SRCTEST))
OBJCODECOV := $(patsubst %.c,$(DIRCODECOV)/%.o,$(SRCTEST))

$(DIRMAIN)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo Using $(CC) to compile $< for main debug..
	@$(CC) -c -I$(INCLUDE) -o $@ $< $(CFLAGS) $(MAINFLAGS)

$(DIRREGEX)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo Using $(CC) to compile $< for regex debug..
	@$(CC) -c -I$(INCLUDE) -o $@ $< $(CFLAGS) $(REGEXFLAGS)

$(DIRTEST)/%.o: %.c
	@echo $@
	@mkdir -p $(dir $@)
	@echo Compiling $< for unit testing..
	@$(CC) -c -I$(INCLUDE) -o $@ $< $(CFLAGS) $(TESTFLAGS)

$(DIRCODECOV)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo Compiling $< for code coverage..
	@$(CC) -c -I$(INCLUDE) -o $@ $< $(CFLAGS) $(CODECOVFLAGS)

$(DIRREL)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo Compiling $< for release..
	@$(CC) -c -I$(INCLUDE) -o $@ $< $(CFLAGS) $(RELFLAGS)

.PHONY: all
all: main regex release test codecoverage

main: $(OBJMAIN)
	@echo Linking $@
	@mkdir -p ${BINDIR}
	@$(CC) $(LFLAGS) $^ -o ./${BINDIR}/$@-BSD.real

regex: $(OBJREGEX)
	@echo Linking $@
	@mkdir -p ${BINDIR}
	@$(CC) $(LFLAGS) $^ -o ./${BINDIR}/$@-BSD.real
	@echo "NOTICE: regex pruning is not implemented yet."

release: $(OBJREL)
	@echo Linking $@
	@mkdir -p ${BINDIR}
	@$(CC) $(FLAGS) $^ -o ./${BINDIR}/$@-BSD.real

fpos: obj/testfpos.o
	@mkdir -p ${BINDIR}
	@$(CC) $(LFLAGS) $^ -o ./${BINDIR}/$@-BSD.real

test: $(OBJTEST)
	@mkdir -p ${BINDIR}
	@$(CC) $(CFLAGS) $(TESTFLAGS) $^ -o ./${BINDIR}/$@-BSD.real

testsizet: $(OBJTEST_SIZE_T)
	@mkdir -p ${BINDIR}
	@$(CC) $(CFLAGS) $(TESTFLAGS) $(SIZE_T) $^ -o ./${BINDIR}/$@-BSD.real

codecoverage: $(OBJCODECOV)
	@mkdir -p ${BINDIR}
	@$(CC) $(CFLAGS) $(CODECOVFLAGS) $^ -o ./${BINDIR}/$@-BSD.real

.PHONY: clean
clean:
	@rm -rf ./$(OBJDIR)
	@rm -rf ./${BINDIR}
