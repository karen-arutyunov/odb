# file      : odb/makefile
# copyright : Copyright (c) 2009-2017 Code Synthesis Tools CC
# license   : GNU GPL v3; see accompanying LICENSE file

include $(dir $(lastword $(MAKEFILE_LIST)))../build/bootstrap.make

# Plugin units.
#
cxx_ptun :=                          \
cxx-lexer.cxx                        \
sql-token.cxx                        \
sql-lexer.cxx                        \
context.cxx                          \
common.cxx                           \
common-query.cxx                     \
location.cxx                         \
diagnostics.cxx                      \
emitter.cxx                          \
lookup.cxx                           \
instance.cxx                         \
include.cxx                          \
header.cxx                           \
inline.cxx                           \
source.cxx                           \
validator.cxx                        \
processor.cxx                        \
generator.cxx                        \
parser.cxx                           \
plugin.cxx                           \
pragma.cxx

# Relational.
#
cxx_ptun +=                          \
relational/changelog.cxx             \
relational/common.cxx                \
relational/common-query.cxx          \
relational/context.cxx               \
relational/processor.cxx             \
relational/header.cxx                \
relational/inline.cxx                \
relational/source.cxx                \
relational/model.cxx                 \
relational/schema.cxx                \
relational/schema-source.cxx         \
relational/validator.cxx

# Relational/MSSQL.
#
cxx_ptun +=                          \
relational/mssql/common.cxx          \
relational/mssql/context.cxx         \
relational/mssql/header.cxx          \
relational/mssql/inline.cxx          \
relational/mssql/source.cxx          \
relational/mssql/model.cxx           \
relational/mssql/schema.cxx

# Relational/MySQL.
#
cxx_ptun +=                          \
relational/mysql/common.cxx          \
relational/mysql/context.cxx         \
relational/mysql/header.cxx          \
relational/mysql/inline.cxx          \
relational/mysql/source.cxx          \
relational/mysql/model.cxx           \
relational/mysql/schema.cxx

# Relational/Oracle
#
cxx_ptun +=                          \
relational/oracle/common.cxx         \
relational/oracle/context.cxx        \
relational/oracle/header.cxx         \
relational/oracle/inline.cxx         \
relational/oracle/source.cxx         \
relational/oracle/model.cxx          \
relational/oracle/schema.cxx

# Relational/PostgreSQL
#
cxx_ptun +=                          \
relational/pgsql/common.cxx          \
relational/pgsql/context.cxx         \
relational/pgsql/header.cxx          \
relational/pgsql/inline.cxx          \
relational/pgsql/source.cxx          \
relational/pgsql/model.cxx           \
relational/pgsql/schema.cxx

# Relational/SQLite.
#
cxx_ptun +=                          \
relational/sqlite/common.cxx         \
relational/sqlite/context.cxx        \
relational/sqlite/header.cxx         \
relational/sqlite/inline.cxx         \
relational/sqlite/source.cxx         \
relational/sqlite/model.cxx          \
relational/sqlite/schema.cxx

# Semantics.
#
cxx_ptun +=                          \
semantics/class.cxx                  \
semantics/class-template.cxx         \
semantics/derived.cxx                \
semantics/elements.cxx               \
semantics/enum.cxx                   \
semantics/fundamental.cxx            \
semantics/namespace.cxx              \
semantics/template.cxx               \
semantics/union.cxx                  \
semantics/union-template.cxx         \
semantics/unit.cxx

# Semantics/Relational.
#
cxx_ptun +=                          \
semantics/relational/changelog.cxx   \
semantics/relational/changeset.cxx   \
semantics/relational/column.cxx      \
semantics/relational/elements.cxx    \
semantics/relational/foreign-key.cxx \
semantics/relational/index.cxx       \
semantics/relational/key.cxx         \
semantics/relational/model.cxx       \
semantics/relational/primary-key.cxx \
semantics/relational/table.cxx

# Traversal.
#
cxx_ptun +=                          \
traversal/class.cxx                  \
traversal/class-template.cxx         \
traversal/derived.cxx                \
traversal/elements.cxx               \
traversal/enum.cxx                   \
traversal/template.cxx               \
traversal/union-template.cxx

# Traversal/Relational.
#
cxx_ptun +=                          \
traversal/relational/changelog.cxx   \
traversal/relational/key.cxx

# Driver units.
#
cxx_dtun := odb.cxx

# Common units.
#
cxx_ctun :=                          \
option-types.cxx                     \
option-functions.cxx                 \
profile.cxx                          \
semantics/relational/name.cxx        \
semantics/relational/deferrable.cxx

# Options file.
#
cli_tun := options.cli

#
#
cxx_pobj  := $(addprefix $(out_base)/,$(cxx_ptun:.cxx=.o))
cxx_dobj  := $(addprefix $(out_base)/,$(cxx_dtun:.cxx=.o))
cxx_cobj  := $(addprefix $(out_base)/,$(cxx_ctun:.cxx=.o) $(cli_tun:.cli=.o))
cxx_pod   := $(cxx_pobj:.o=.o.d)
cxx_dod   := $(cxx_dobj:.o=.o.d)
cxx_cod   := $(cxx_cobj:.o=.o.d)


odb      := $(out_base)/odb
odb_so   := $(out_base)/odb.so

# Dummy library to force driver timestamp update when the plugin DSO
# changes.
#
odb.l := $(out_base)/odb.l
clean := $(out_base)/.clean
dist  := $(out_base)/.dist

# Import.
#
$(call import,\
  $(scf_root)/import/cli/stub.make,\
  cli: cli,cli-rules: cli_rules)

$(call import,\
  $(scf_root)/import/libcutl/stub.make,\
  l: cutl.l,cpp-options: cutl.l.cpp-options)

# Build.
#
$(odb): $(cxx_dobj) $(cxx_cobj) $(odb.l) $(cutl.l)
$(odb_so): $(cxx_pobj) $(cxx_cobj) $(cutl.l)

$(cxx_pobj) $(cxx_dobj) $(cxx_cobj) $(cxx_pod) $(cxx_dod) $(cxx_cod): \
$(cutl.l.cpp-options)

genf := $(cli_tun:.cli=.hxx) $(cli_tun:.cli=.ixx) $(cli_tun:.cli=.cxx)
gen  := $(addprefix $(out_base)/,$(genf))

# Don't try to depend on the installed executable.
#
ifneq ($(cli),cli)
$(gen): $(cli)
endif

$(gen): cli := $(cli)
$(gen): cli_options +=  \
--generate-modifier     \
--generate-specifier    \
--generate-description  \
--suppress-undocumented \
--generate-file-scanner \
--include-with-brackets \
--include-prefix odb    \
--guard-prefix ODB      \
--cxx-prologue '\#include <odb/option-parsers.hxx>'

$(call include-dep,$(cxx_pod) $(cxx_dod) $(cxx_cod),\
$(cxx_pobj) $(cxx_dobj) $(cxx_cobj),$(gen))

# Alias for default target.
#
$(out_base)/: $(odb)

# Install.
#
$(install): $(odb)
	$(call install-exec,$<,$(install_bin_dir)/odb)
	$(call install-exec,$<.so,$(install_bin_dir)/odb.so)

# Dist.
#
$(dist): export plugin_sources := $(cxx_ptun)
$(dist): export driver_sources := $(cxx_dtun)
$(dist): common_sources_dist := $(cxx_ctun)
$(dist): export common_sources := $(common_sources_dist) $(cli_tun:.cli=.cxx)
$(dist): headers_dist = $(subst $(src_base)/,,$(shell find $(src_base) \
-name '*.hxx' -o -name '*.ixx' -o -name '*.txx'))
$(dist): gen_headers := $(cli_tun:.cli=.hxx) $(cli_tun:.cli=.ixx)
$(dist): export headers = $(sort $(headers_dist) $(gen_headers))
$(dist): gen_dist := $(gen)
$(dist): data_dist := $(cli_tun)
$(dist): export extra_dist := $(data_dist)

$(dist): $(gen)
	$(call dist-data,$(plugin_sources) $(driver_sources) \
$(common_sources_dist) $(headers_dist) $(gen_dist) $(data_dist))
	$(call meta-automake)

# Clean.
#
$(clean):                             \
  $(odb).o.clean                      \
  $(addsuffix .cxx.clean,$(cxx_pobj)) \
  $(addsuffix .cxx.clean,$(cxx_dobj)) \
  $(addsuffix .cxx.clean,$(cxx_cobj)) \
  $(addsuffix .cxx.clean,$(cxx_pod))  \
  $(addsuffix .cxx.clean,$(cxx_dod))  \
  $(addsuffix .cxx.clean,$(cxx_cod))  \
  $(addprefix $(out_base)/,$(cli_tun:.cli=.cxx.cli.clean))
	$(call message,rm $$1,rm -f $$1,$(out_base)/odb.so)
	$(call message,,rm -f $(out_base)/odb.l)

# Generated .gitignore.
#
ifeq ($(out_base),$(src_base))
$(odb): | $(out_base)/.gitignore

$(out_base)/.gitignore: files := odb odb.so $(genf)
$(clean): $(out_base)/.gitignore.clean

$(call include,$(bld_root)/git/gitignore.make)
endif

# Rules.
#
$(call include,$(bld_root)/dist.make)
$(call include,$(bld_root)/meta/automake.make)
$(call include,$(bld_root)/install.make)

$(call include,$(cli_rules))
$(call include,$(bld_root)/cxx/cxx-d.make)
$(call include,$(bld_root)/cxx/cxx-o.make)
$(call include,$(bld_root)/cxx/o-e.make)

# Custom rules for the plugin and the driver.
#
ifdef cxx_gnu

$(cxx_pobj) $(cxx_cobj): cxx_pic_options := -fPIC

$(cxx_cobj) $(cxx_cod): cpp_options := -I$(src_root)
$(cxx_dobj) $(cxx_dod): cpp_options := -I$(src_root) '-DODB_GXX_NAME="$(cxx_gnu)"'
$(cxx_pobj) $(cxx_pod): cpp_options := -I$(src_root) \
-I$(shell $(cxx_gnu) -print-file-name=plugin)/include

$(odb_so):
	$(call message,ld $@,$(ld) \
$(cxx_extra_options) $(ld_options) $(cxx_ld_extra_options) -shared -o $@ \
$(foreach f,$^,$(if $(patsubst %.l,,$f),$f,$(call expand-l,$f))) $(cxx_extra_libs))

$(odb.l): $(odb_so)
	 $(call message,,touch $@)

endif
