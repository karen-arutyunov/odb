# file      : makefile
# author    : Boris Kolpackov <boris@codesynthesis.com>
# copyright : Copyright (c) 2005-2010 Code Synthesis Tools CC
# license   : GNU GPL v2; see accompanying LICENSE file

include $(dir $(lastword $(MAKEFILE_LIST)))build/bootstrap.make

default  := $(out_base)/
test     := $(out_base)/.test
install  := $(out_base)/.install
clean    := $(out_base)/.clean
cleandoc := $(out_base)/.cleandoc

$(default): $(out_base)/odb/


# Test.
#
$(test): $(out_base)/tests/.test


# Install.
#
$(install): $(out_base)/odb/.install


# Clean.
#
$(clean): $(out_base)/odb/.clean

$(cleandoc): $(out_base)/doc/.cleandoc

$(call include,$(bld_root)/install.make)

$(call import,$(src_base)/odb/makefile)
