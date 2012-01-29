# file      : build/import/libsqlite/configuration-rules.make
# copyright : Copyright (c) 2009-2011 Boris Kolpackov
# license   : GNU GPL v2; see accompanying LICENSE file

$(dcf_root)/import/libsqlite/configuration-dynamic.make: | $(dcf_root)/import/libsqlite/.
	$(call message,,$(scf_root)/import/libsqlite/configure $@)

ifndef %foreign%

disfigure::
	$(call message,rm $(dcf_root)/import/libsqlite/configuration-dynamic.make,\
rm -f $(dcf_root)/import/libsqlite/configuration-dynamic.make)

endif
