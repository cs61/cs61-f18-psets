# Default optimization level
O ?= 2

all: sh61

-include build/rules.mk

%.o: %.cc sh61.hh $(BUILDSTAMP)
	$(call run,$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS) $(O) -o $@ -c,COMPILE,$<)

sh61: sh61.o helpers.o
	$(call run,$(CXX) $(CXXFLAGS) $(O) -o $@ $^ $(LDFLAGS) $(LIBS),LINK $@)

sleep61: sleep61.cc
	$(call run,$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS) $(O) -o $@ $^ $(LDFLAGS) $(LIBS),BUILD $@)

ifneq ($(filter -fsanitize=leak,$(CXXFLAGS)),)
LEAKCHECK = --leak
endif

check: sh61
	perl check.pl $(LEAKCHECK)

check-%: sh61
	perl check.pl $(LEAKCHECK) $(subst check-,,$@)

clean: clean-main
clean-main:
	$(call run,rm -f sh61 *.o *~ *.bak core *.core,CLEAN)
	$(call run,rm -rf out *.dSYM $(DEPSDIR))

.PRECIOUS: %.o
.PHONY: all clean clean-main distclean check check-%
