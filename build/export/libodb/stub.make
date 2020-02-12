# file      : build/export/libodb/stub.make
# license   : GNU GPL v2; see accompanying LICENSE file

$(call include-once,$(src_root)/odb/makefile,$(out_root))

$(call export,\
  l: $(out_root)/odb/odb.l,\
  cpp-options: $(out_root)/odb/odb.l.cpp-options)
