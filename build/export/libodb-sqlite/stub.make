# file      : build/export/libodb-sqlite/stub.make
# copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
# license   : GNU GPL v2; see accompanying LICENSE file

$(call include-once,$(src_root)/odb/sqlite/makefile,$(out_root))

$(call export,\
  l: $(out_root)/odb/sqlite/odb-sqlite.l,\
  cpp-options: $(out_root)/odb/sqlite/odb-sqlite.l.cpp-options)
