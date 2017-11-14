# file      : doc/makefile
# copyright : Copyright (c) 2009-2017 Code Synthesis Tools CC
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
$(default):                \
$(out_base)/odb.1          \
$(out_base)/odb.xhtml      \
$(out_base)/odb-manual.ps  \
$(out_base)/odb-manual.pdf

# Man/html pages.
#
$(out_base)/odb.xhtml $(out_base)/odb.1: cli := $(cli)

$(out_base)/odb.xhtml: $(src_root)/odb/options.cli    \
                       $(src_base)/odb-prologue.xhtml \
                       $(src_base)/odb-epilogue.xhtml | $(out_base)/.
	$(call message,cli-html $<,$(cli) --generate-html --stdout \
--suppress-undocumented \
--html-prologue-file $(src_base)/odb-prologue.xhtml \
--html-epilogue-file $(src_base)/odb-epilogue.xhtml $< >$@)

$(out_base)/odb.1: $(src_root)/odb/options.cli \
                   $(src_base)/odb-prologue.1  \
                   $(src_base)/odb-epilogue.1 | $(out_base)/.
	$(call message,cli-man $<,$(cli) --generate-man --stdout \
--suppress-undocumented \
--man-prologue-file $(src_base)/odb-prologue.1 \
--man-epilogue-file $(src_base)/odb-epilogue.1 $< >$@)

# Manual.
#
$(out_base)/odb-manual.ps: $(src_base)/manual.xhtml   \
                           $(src_base)/manual.html2ps \
                           $(src_base)/odb-arch.png   \
                           $(src_base)/odb-flow.png | $(out_base)/.
	$(call message,html2ps $<,html2ps -f $(src_base)/manual.html2ps -o $@ $<)

$(out_base)/odb-manual.pdf: $(out_base)/odb-manual.ps
	$(call message,ps2pdf $<,ps2pdf14 $< $@)

# Dist.
#
$(dist): data_dist := default.css manual.xhtml odb-arch.png odb-flow.png
$(dist): export ps_docs   := odb-manual.ps
$(dist): export pdf_docs  := odb-manual.pdf
$(dist): export html_docs := $(data_dist) odb.xhtml
$(dist): export man_docs  := odb.1
$(dist):                   \
$(out_base)/odb.1          \
$(out_base)/odb.xhtml      \
$(out_base)/odb-manual.ps  \
$(out_base)/odb-manual.pdf
	$(call dist-data,$^)
	$(call dist-data,$(data_dist))
	$(call meta-automake)

# Clean.
#
$(clean):
	$(call message,rm $$1,rm -f $$1,$(out_base)/odb.1)
	$(call message,rm $$1,rm -f $$1,$(out_base)/odb.xhtml)
	$(call message,rm $$1,rm -f $$1,$(out_base)/odb-manual.ps)
	$(call message,rm $$1,rm -f $$1,$(out_base)/odb-manual.pdf)

$(call include,$(bld_root)/dist.make)
$(call include,$(bld_root)/meta/automake.make)
