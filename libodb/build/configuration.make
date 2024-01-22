# file      : build/configuration.make
# license   : GNU GPL v2; see accompanying LICENSE file

$(call include-once,$(scf_root)/configuration-rules.make,$(dcf_root))

# Dynamic configuration.
#
libodb_threads :=

$(call -include,$(dcf_root)/configuration-dynamic.make)

ifdef libodb_threads

$(out_root)/%: libodb_threads := $(libodb_threads)

else

.NOTPARALLEL:

endif
