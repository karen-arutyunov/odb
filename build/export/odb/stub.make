# file      : build/export/odb/stub.make
# copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
# license   : GNU GPL v3; see accompanying LICENSE file

$(call include-once,$(src_root)/odb/makefile,$(out_root))

# Use the rules file from odb's import directory instead of the
# importing project's one.
#
$(call export,\
  odb: $(out_root)/odb/odb,\
  odb-rules: $(scf_root)/import/odb/hxx-cxx.make)
