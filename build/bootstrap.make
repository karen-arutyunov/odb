# file      : build/bootstrap.make
# author    : Boris Kolpackov <boris@codesynthesis.com>
# copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
# license   : GNU GPL v3; see accompanying LICENSE file

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
        $(out_base)/.dist     \
        $(out_base)/.clean    \
	$(out_base)/.cleandoc

ifdef %interactive%

.PHONY: dist clean cleandoc

dist:     $(out_base)/.dist
clean:    $(out_base)/.clean
cleandoc: $(out_base)/.cleandoc

endif

# Make sure the distribution prefix is set if the goal is dist.
#
ifneq ($(filter $(MAKECMDGOALS),dist),)
ifeq ($(dist_prefix),)
$(error dist_prefix is not set)
endif
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
ifneq ($(filter $(MAKECMDGOALS),clean cleandoc disfigure dist),)
include-dep =
endif

.DEFAULT_GOAL := $(def_goal)
