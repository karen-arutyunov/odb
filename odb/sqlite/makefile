# file      : odb/sqlite/makefile
# copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
# license   : GNU GPL v2; see accompanying LICENSE file

include $(dir $(lastword $(MAKEFILE_LIST)))../../build/bootstrap.make

cxx :=                       \
connection.cxx               \
connection-factory.cxx       \
database.cxx                 \
error.cxx                    \
exceptions.cxx               \
prepared-query.cxx           \
query.cxx                    \
query-dynamic.cxx            \
query-const-expr.cxx         \
simple-object-statements.cxx \
statement.cxx                \
statements-base.cxx          \
statement-cache.cxx          \
tracer.cxx                   \
traits.cxx                   \
transaction.cxx              \
transaction-impl.cxx

cli_tun := details/options.cli
cxx_tun := $(cxx)
cxx_obj := $(addprefix $(out_base)/,$(cxx_tun:.cxx=.o) $(cli_tun:.cli=.o))
cxx_od  := $(cxx_obj:.o=.o.d)

odb_sqlite.l             := $(out_base)/odb-sqlite.l
odb_sqlite.l.cpp-options := $(out_base)/odb-sqlite.l.cpp-options

default := $(out_base)/
dist    := $(out_base)/.dist
clean   := $(out_base)/.clean

# Import.
#
$(call import,\
  $(scf_root)/import/cli/stub.make,\
  cli: cli,cli-rules: cli_rules)

$(call import,\
  $(scf_root)/import/libodb/stub.make,\
  l: odb.l,cpp-options: odb.l.cpp-options)

$(call import,\
  $(scf_root)/import/libsqlite/stub.make,\
  l: sqlite.l,cpp-options: sqlite.l.cpp-options)

# Build.
#
$(odb_sqlite.l): $(cxx_obj) $(odb.l) $(sqlite.l)
$(odb_sqlite.l.cpp-options): value := -I$(out_root) -I$(src_root)
$(odb_sqlite.l.cpp-options): $(odb.l.cpp-options) $(sqlite.l.cpp-options)

$(cxx_obj) $(cxx_od): $(odb_sqlite.l.cpp-options) $(out_base)/details/config.h

genf := $(cli_tun:.cli=.hxx) $(cli_tun:.cli=.ixx) $(cli_tun:.cli=.cxx)
gen  := $(addprefix $(out_base)/,$(genf))

$(gen): $(cli)
$(gen): cli := $(cli)
$(gen): cli_options +=                   \
--long-usage                             \
--generate-file-scanner                  \
--cli-namespace odb::sqlite::details::cli \
--guard-prefix LIBODB_SQLITE_DETAILS

$(out_base)/details/config.h: | $(out_base)/details/.
	@echo '/* file      : odb/sqlite/details/config.h'               >$@
	@echo ' * note      : automatically generated'                  >>$@
	@echo ' */'                                                     >>$@
	@echo ''                                                        >>$@
	@echo '#ifndef ODB_SQLITE_DETAILS_CONFIG_H'                     >>$@
	@echo '#define ODB_SQLITE_DETAILS_CONFIG_H'                     >>$@
	@echo ''                                                        >>$@
	@echo '#include <sqlite3.h>'                                    >>$@
	@echo ''                                                        >>$@
	@echo '#if SQLITE_VERSION_NUMBER >= 3006012'                    >>$@
	@echo '  #define LIBODB_SQLITE_HAVE_UNLOCK_NOTIFY 1'            >>$@
	@echo '#endif'                                                  >>$@
	@echo ''                                                        >>$@
	@echo '#endif /* ODB_SQLITE_DETAILS_CONFIG_H */'                >>$@

$(call include-dep,$(cxx_od),$(cxx_obj),$(gen) $(out_base)/details/config.h)

# Convenience alias for default target.
#
$(out_base)/: $(odb_sqlite.l)

# Dist.
#
$(dist): sources_dist := $(cxx)
$(dist): export sources := $(sources_dist) $(cli_tun:.cli=.cxx)
$(dist): headers_dist = $(subst $(src_base)/,,$(shell find $(src_base) \
-name '*.hxx' -o -name '*.ixx' -o -name '*.txx'))
$(dist): gen_headers := $(cli_tun:.cli=.hxx) $(cli_tun:.cli=.ixx)
$(dist): export headers = $(sort $(headers_dist) $(gen_headers))
$(dist): gen_dist := $(gen)
$(dist): data_dist := $(cli_tun) details/config.h.in
$(dist): export extra_dist := $(data_dist)                    \
libodb-sqlite-vc8.vcproj libodb-sqlite-vc9.vcproj             \
libodb-sqlite-vc10.vcxproj libodb-sqlite-vc10.vcxproj.filters \
libodb-sqlite-vc11.vcxproj libodb-sqlite-vc11.vcxproj.filters
$(dist): export interface_version = $(shell sed -e \
's/^\([0-9]*\.[0-9]*\).*/\1/' $(src_root)/version)

$(dist): $(gen)
	$(call dist-data,$(sources_dist) $(headers_dist) $(data_dist))
	$(call dist-data,$(gen_dist),$(dist_prefix)/odb/sqlite/details)
	$(call meta-vc8proj,$(src_base)/libodb-sqlite-vc8.vcproj)
	$(call meta-vc9proj,$(src_base)/libodb-sqlite-vc9.vcproj)
	$(call meta-vc10proj,$(src_base)/libodb-sqlite-vc10.vcxproj)
	$(call meta-vc11proj,$(src_base)/libodb-sqlite-vc11.vcxproj)
	$(call meta-automake)

# Clean.
#
$(clean): $(odb_sqlite.l).o.clean    \
  $(odb_sqlite.l.cpp-options).clean  \
  $(addsuffix .cxx.clean,$(cxx_obj)) \
  $(addsuffix .cxx.clean,$(cxx_od))  \
  $(addprefix $(out_base)/,$(cli_tun:.cli=.cxx.cli.clean))
	$(call message,rm $$1,rm -f $$1,$(out_base)/details/config.h)

# Generated .gitignore.
#
ifeq ($(out_base),$(src_base))
$(odb_sqlite.l): | $(out_base)/.gitignore

$(out_base)/.gitignore: files := details/config.h $(genf)
$(clean): $(out_base)/.gitignore.clean

$(call include,$(bld_root)/git/gitignore.make)
endif

# How to.
#
$(call include,$(bld_root)/dist.make)
$(call include,$(bld_root)/meta/vc8proj.make)
$(call include,$(bld_root)/meta/vc9proj.make)
$(call include,$(bld_root)/meta/vc10proj.make)
$(call include,$(bld_root)/meta/vc11proj.make)
$(call include,$(bld_root)/meta/automake.make)

$(call include,$(cli_rules))
$(call include,$(bld_root)/cxx/o-l.make)
$(call include,$(bld_root)/cxx/cxx-o.make)
$(call include,$(bld_root)/cxx/cxx-d.make)
