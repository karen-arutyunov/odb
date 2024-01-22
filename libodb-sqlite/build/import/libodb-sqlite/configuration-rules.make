# file      : build/import/libodb-sqlite/configuration-rules.make
# license   : GNU GPL v2; see accompanying LICENSE file

$(dcf_root)/import/libodb-sqlite/configuration-dynamic.make: | $(dcf_root)/import/libodb-sqlite/.
	$(call message,,$(scf_root)/import/libodb-sqlite/configure $@)

ifndef %foreign%

$(dcf_root)/.disfigure::
	$(call message,rm $(dcf_root)/import/libodb-sqlite/configuration-dynamic.make,\
rm -f $(dcf_root)/import/libodb-sqlite/configuration-dynamic.make)

endif
