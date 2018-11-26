# Default optimization level
O ?= 2

all: pong61 simpong61

-include build/rules.mk

LIBS = -lpthread -lm

%.o: %.cc $(BUILDSTAMP)
	$(call run,$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPCFLAGS) $(O) -o $@ -c,COMPILE,$<)

pong61: pong61.o
	$(call run,$(CXX) $(CXXFLAGS) $(O) -o $@ $^ $(LDFLAGS) $(LIBS),LINK $@)

simpong61: simpong61.o
	$(call run,$(CXX) $(CXXFLAGS) $(O) -o $@ $^ $(LDFLAGS) $(LIBS),LINK $@)

run: pong61
	./pong61

clean: clean-main
clean-main:
	$(call run,rm -rf pong61 *.o *~ *.bak core *.core,CLEAN)
	$(call run,rm -rf out *.dSYM $(DEPSDIR))

.PRECIOUS: %.o
.PHONY: all clean clean-main clean-hook distclean run check check-% prepare-check
