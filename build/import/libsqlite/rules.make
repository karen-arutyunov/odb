# file      : build/import/libsqlite/rules.make
# copyright : Copyright (c) 2009-2015 Code Synthesis Tools CC
# license   : GNU GPL v2; see accompanying LICENSE file

$(dcf_root)/import/libsqlite/%: root := $(libsqlite_root)

ifeq ($(libsqlite_type),archive)

$(dcf_root)/import/libsqlite/sqlite.l: $(libsqlite_root)/.libs/libsqlite3.a
	@echo $< >$@
else

$(dcf_root)/import/libsqlite/sqlite.l: $(libsqlite_root)/.libs/libsqlite3.so
	@echo $< >$@
	@echo rpath:$(root)/.libs >>$@
endif

$(dcf_root)/import/libsqlite/sqlite.l.cpp-options:
	@echo include: -I$(root) >$@

ifndef %foreign%

disfigure::
	$(call message,rm $(dcf_root)/import/libsqlite/sqlite.l,\
rm -f $(dcf_root)/import/libsqlite/sqlite.l)
	$(call message,,rm -f $(dcf_root)/import/libsqlite/sqlite.l.cpp-options)

endif
