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

check: sh61
	perl check.pl

check-%: sh61
	perl check.pl $(subst check-,,$@)

clean: clean-main
clean-main:
	$(call run,rm -f sh61 *.o *~ *.bak core *.core,CLEAN)
	$(call run,rm -rf out *.dSYM $(DEPSDIR))

realclean: clean
	@echo + realclean
	$(V)rm -rf $(DISTDIR) $(DISTDIR).tar.gz

.PRECIOUS: %.o
.PHONY: all clean clean-main check check-%
