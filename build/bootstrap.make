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

# If we don't have dependency auto-generation then we need to manually
# make sure that C++ header files are compiled before C++ source files.
# To do this we make the object files ($2) depend in order-only on
# generated files ($3).
#
ifeq ($(cxx_id),generic)

define include-dep
$(if $2,$(eval $2: | $3))
endef

else

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
