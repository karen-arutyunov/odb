# file      : doc/makefile
# author    : Boris Kolpackov <boris@codesynthesis.com>
# copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
# license   : GNU GPL v3; see accompanying LICENSE file

include $(dir $(lastword $(MAKEFILE_LIST)))../build/bootstrap.make

default := $(out_base)/
dist    := $(out_base)/.dist
clean   := $(out_base)/.clean

# Import.
#
$(call import,\
  $(scf_root)/import/cli/stub.make,\
  cli: cli,cli-rules: cli_rules)

# Build.
#
$(default): $(out_base)/odb.xhtml $(out_base)/odb.1

$(out_base)/odb.xhtml $(out_base)/odb.1: cli := $(cli)

$(out_base)/odb.xhtml: $(src_root)/odb/options.cli    \
                       $(src_base)/odb-prologue.xhtml \
                       $(src_base)/odb-epilogue.xhtml
	$(call message,cli-html $<,$(cli) --generate-html --stdout \
--html-prologue $(src_base)/odb-prologue.xhtml \
--html-epilogue $(src_base)/odb-epilogue.xhtml $< >$@)

$(out_base)/odb.1: $(src_root)/odb/options.cli    \
                       $(src_base)/odb-prologue.1 \
                       $(src_base)/odb-epilogue.1
	$(call message,cli-man $<,$(cli) --generate-man --stdout \
--man-prologue $(src_base)/odb-prologue.1 \
--man-epilogue $(src_base)/odb-epilogue.1 $< >$@)

# Dist.
#
$(dist): data_dist := default.css
$(dist): export docs := odb.xhtml
$(dist): export mans := odb.1
$(dist): export extra_dist := $(data_dist)
$(dist): $(out_base)/odb.xhtml $(out_base)/odb.1
	$(call dist-data,$(data_dist))
	$(call dist-data,$(out_base)/odb.1)
	$(call dist-data,$(out_base)/odb.xhtml)
	$(call meta-automake)

# Clean.
#
$(clean):
	$(call message,rm $$1,rm -f $$1,$(out_base)/odb.xhtml)
	$(call message,rm $$1,rm -f $$1,$(out_base)/odb.1)

# Generated .gitignore.
#
ifeq ($(out_base),$(src_base))
$(out_base)/odb.xhtml $(out_base)/odb.1: | $(out_base)/.gitignore

$(out_base)/.gitignore: files := odb.xhtml odb.1
$(clean): $(out_base)/.gitignore.clean

$(call include,$(bld_root)/git/gitignore.make)
endif

$(call include,$(bld_root)/dist.make)
$(call include,$(bld_root)/meta/automake.make)
