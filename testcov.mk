include Makefile

BIGTEST := -o .bigin tests/001_inputs/bcan_anudeepND_ads.fat tests/001_inputs/EasyList_France.fat tests/001_inputs/ccan_StevenBlack_hosts.fat tests/001_inputs/acan_hagezi_pro_plus.fat
SMLTEST := -o .smallin tests/001_inputs/bcan_anudeepND_ads.fat

VALGRIND := valgrind
LEAKARGS := -s --track-origins=yes --tool=memcheck --leak-check=full

valgrind: main
	@$(VALGRIND) $(LEAKARGS) ./${BINDIR}/$<.real $(SMLTEST)

valgrindbig: main
	@$(VALGRIND) $(LEAKARGS) ./${BINDIR}/$<.real $(BIGTEST)

valgrindtest: test
	@$(VALGRIND) $(LEAKARGS) ./${BINDIR}/$<.real -t

cachegrindbig: main
	@$(VALGRIND) --tool=cachegrind --log-file=cachegrindbig.log ./${BINDIR}/$<.real $(BIGTEST)

callgrindbig: main
	@$(VALGRIND) --tool=callgrind --log-file=callgrindbig.log ./${BINDIR}/$<.real $(BIGTEST)

mainbig: main
	time ./${BINDIR}/$<.real $(BIGTEST)

regexbig: regex
	time ./${BINDIR}/$<.real $(BIGTEST)

valgrindregex: regex
	@$(VALGRIND) $(LEAKARGS) ./${BINDIR}/$<.real $(SMLTEST)

realbig: release
	time ./${BINDIR}/$<.real $(BIGTEST)

