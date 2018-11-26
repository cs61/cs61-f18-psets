# are we using clang?
ISCLANG := $(shell if $(CC) --version | grep -e 'LLVM\|clang' >/dev/null; then echo 1; fi)
ISLINUX := $(if $(wildcard /usr/include/linux/*.h),1,)

CFLAGS := -std=gnu11 -W -Wall -Wshadow -g $(DEFS) $(CFLAGS)
CXXFLAGS := -std=gnu++1z -W -Wall -Wshadow -g $(DEFS) $(CXXFLAGS)
O ?= -O3
ifeq ($(filter 0 1 2 3 s,$(O)$(NOOVERRIDEO)),$(strip $(O)))
override O := -O$(O)
endif

# sanitizer arguments
ifndef SAN
SAN := $(or $(SANITIZE),$(ASAN),$(LSAN),$(LEAKSAN),$(TSAN),$(UBSAN))
endif
WANT_TSAN ?= 1
ifndef TSAN
 ifeq ($(WANT_TSAN),1)
TSAN := $(SAN)
 endif
endif

check_for_sanitizer = $(if $(strip $(shell $(CC) -fsanitize=$(1) -x c -E /dev/null 2>&1 | grep sanitize=)),$(info ** WARNING: The `$(CC)` compiler does not support `-fsanitize=$(1)`.),1)
ifeq ($(TSAN),1)
 ifeq ($(or $(ISCLANG),$(filter-out 3.% 4.% 5.% 6.%,$(shell $(CC) -dumpversion)),$(filter-out Linux%,$(shell uname))),)
$(info ** WARNING: If ThreadSanitizer fails, try `make SAN=1 CC=clang`.)
 endif
 ifeq ($(call check_for_sanitizer,thread),1)
CFLAGS += -fsanitize=thread
CXXFLAGS += -fsanitize=thread
 endif
else
 ifeq ($(or $(ASAN),$(SAN)),1)
  ifeq ($(call check_for_sanitizer,address),1)
CFLAGS += -fsanitize=address
CXXFLAGS += -fsanitize=address
  endif
 endif
 ifeq ($(or $(LSAN),$(LEAKSAN)),1)
  ifeq ($(call check_for_sanitizer,leak),1)
CFLAGS += -fsanitize=leak
CXXFLAGS += -fsanitize=leak
  endif
 endif
endif
ifeq ($(or $(UBSAN),$(SAN)),1)
 ifeq ($(call check_for_sanitizer,undefined),1)
CFLAGS += -fsanitize=undefined
CXXFLAGS += -fsanitize=undefined
 endif
endif

# profiling
ifeq ($(or $(PROFILE),$(PG)),1)
CFLAGS += -pg
CXXFLAGS += -pg
endif

# these rules ensure dependencies are created
DEPCFLAGS = -MD -MF $(DEPSDIR)/$*.d -MP
DEPSDIR := .deps
BUILDSTAMP := $(DEPSDIR)/rebuildstamp
DEPFILES := $(wildcard $(DEPSDIR)/*.d)
ifneq ($(DEPFILES),)
include $(DEPFILES)
endif

# when the C compiler or optimization flags change, rebuild all objects
ifneq ($(strip $(DEP_CC)),$(strip $(CC) $(CPPFLAGS) $(CFLAGS) $(O)))
DEP_CC := $(shell mkdir -p $(DEPSDIR); echo >$(BUILDSTAMP); echo "DEP_CC:=$(CC) $(CPPFLAGS) $(CFLAGS) $(O)" >$(DEPSDIR)/_cc.d)
endif
ifneq ($(strip $(DEP_CXX)),$(strip $(CXX) $(CPPFLAGS) $(CXXFLAGS) $(O)))
DEP_CXX := $(shell mkdir -p $(DEPSDIR); echo >$(BUILDSTAMP); echo "DEP_CXX:=$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(O)" >$(DEPSDIR)/_cxx.d)
endif


V = 0
ifeq ($(V),1)
run = $(1) $(3)
xrun = /bin/echo "$(1) $(3)" && $(1) $(3)
else
run = @$(if $(2),/bin/echo "  $(2) $(3)" &&,) $(1) $(3)
xrun = $(if $(2),/bin/echo "  $(2) $(3)" &&,) $(1) $(3)
endif
runquiet = @$(1) $(3)

# cancel implicit rules we don't want
%: %.c
%.o: %.c
%: %.cc
%.o: %.cc
%: %.o

$(BUILDSTAMP):
	@mkdir -p $(@D)
	@echo >$@

always:
	@:

clean-hook:
	@:

.PHONY: always clean-hook
.PRECIOUS: %.o
