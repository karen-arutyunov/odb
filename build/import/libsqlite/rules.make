# file      : build/import/libsqlite/rules.make
# author    : Boris Kolpackov <boris@codesynthesis.com>
# copyright : Copyright (c) 2009-2011 Boris Kolpackov
# license   : GNU GPL v2; see accompanying LICENSE file

$(dcf_root)/import/libsqlite/%: root := $(libsqlite_root)

ifeq ($(libsqlite_type),archive)

$(dcf_root)/import/libsqlite/sqlite.l: $(libsqlite_root)/.libs/libsqlite3.a
	@echo $< >$@
else

$(dcf_root)/import/libsqlite/sqlite.l: $(libsqlite_root)/.libs/libsqlite3.so
	@echo $< >$@
	@echo rpath:$(root) >>$@
endif

$(dcf_root)/import/libsqlite/sqlite.l.cpp-options:
	@echo include: -I$(root) >$@

ifndef %foreign%

disfigure::
	$(call message,rm $(dcf_root)/import/libsqlite/sqlite.l,\
rm -f $(dcf_root)/import/libsqlite/sqlite.l)
	$(call message,,rm -f $(dcf_root)/import/libsqlite/sqlite.l.cpp-options)

endif
