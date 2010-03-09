# file      : build/bootstrap.make
# author    : Boris Kolpackov <boris@codesynthesis.com>
# copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
# license   : GNU GPL v2; see accompanying LICENSE file

project_name := odb

# First try to include the bundled bootstrap.make if it exist. If that
# fails, let make search for the external bootstrap.make.
#
build := build-0.3

-include $(dir $(lastword $(MAKEFILE_LIST)))../../$(build)/bootstrap.make

ifeq ($(patsubst %build/bootstrap.make,,$(lastword $(MAKEFILE_LIST))),)
include $(build)/bootstrap.make
endif

def_goal := $(.DEFAULT_GOAL)

# Include C++ configuration. We need to know if we are using the generic
# C++ compiler in which case we need to compensate for missing dependency
# auto-generation (see below).
#
$(call include,$(bld_root)/cxx/configuration.make)

# Aliases
#
.PHONY: $(out_base)/          \
        $(out_base)/.test     \
        $(out_base)/.install  \
        $(out_base)/.clean    \
	$(out_base)/.cleandoc

ifdef %interactive%

.PHONY: test install clean cleandoc

test: $(out_base)/.test
install: $(out_base)/.install
clean: $(out_base)/.clean
cleandoc: $(out_base)/.cleandoc

endif

ifdef cxx_id

# It would be better to do these checks in the script once instead
# of for every makefile.
#
ifneq ($(MAKECMDGOALS),disfigure)

ifneq ($(cxx_id),gnu)
$(error only GNU g++ can be used to build the ODB compiler)
endif

$(call include,$(bld_root)/cxx/gnu/configuration.make)

ifdef cxx_gnu
ifeq ($(shell $(cxx_gnu) -print-file-name=plugin),plugin)
$(error $(cxx_gnu) does not support plugins)
endif
endif
endif # disfigure

define include-dep
$(call -include,$1)
endef

endif


# Don't include dependency info for certain targets.
#
ifneq ($(filter $(MAKECMDGOALS),clean cleandoc disfigure),)
include-dep =
endif

# For install, don't include dependecies in examples, and tests.
#
ifneq ($(filter $(MAKECMDGOALS),install),)

ifneq ($(subst $(src_root)/tests/,,$(src_base)),$(src_base))
include-dep =
endif

ifneq ($(subst $(src_root)/examples/,,$(src_base)),$(src_base))
include-dep =
endif

endif

.DEFAULT_GOAL := $(def_goal)
