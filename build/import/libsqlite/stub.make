# file      : build/import/libsqlite/stub.make
# license   : GNU GPL v2; see accompanying LICENSE file

$(call include-once,$(scf_root)/import/libsqlite/configuration-rules.make,$(dcf_root))

libsqlite_installed :=

$(call -include,$(dcf_root)/import/libsqlite/configuration-dynamic.make)

ifdef libsqlite_installed

ifeq ($(libsqlite_installed),y)

$(call export,l: -lsqlite3,cpp-options: )

else

$(call include-once,$(scf_root)/import/libsqlite/rules.make,$(dcf_root))

$(call export,\
  l: $(dcf_root)/import/libsqlite/sqlite.l,\
  cpp-options: $(dcf_root)/import/libsqlite/sqlite.l.cpp-options)

endif

else

.NOTPARALLEL:

endif
