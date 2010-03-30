# file      : odb/makefile
# author    : Boris Kolpackov <boris@codesynthesis.com>
# copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
# license   : GNU GPL v2; see accompanying LICENSE file

include $(dir $(lastword $(MAKEFILE_LIST)))../build/bootstrap.make

# Plugin units.
#
cxx_ptun :=                  \
context.cxx                  \
generator.cxx                \
plugin.cxx

cxx_ptun +=                  \
semantics/class.cxx          \
semantics/class-template.cxx \
semantics/derived.cxx        \
semantics/elements.cxx       \
semantics/enum.cxx           \
semantics/fundamental.cxx    \
semantics/namespace.cxx      \
semantics/template.cxx       \
semantics/union.cxx          \
semantics/union-template.cxx \
semantics/unit.cxx

cxx_ptun +=                  \
traversal/class.cxx          \
traversal/class-template.cxx \
traversal/derived.cxx        \
traversal/elements.cxx       \
traversal/enum.cxx           \
traversal/template.cxx       \
traversal/union-template.cxx


# Driver units.
#
cxx_dtun := odb.cxx

# Options file.
#
cli_tun := options.cli

#
#
cxx_pobj  := $(addprefix $(out_base)/,$(cxx_ptun:.cxx=.o) $(cli_tun:.cli=.o))
cxx_dobj  := $(addprefix $(out_base)/,$(cxx_dtun:.cxx=.o) $(cli_tun:.cli=.o))
cxx_pod   := $(cxx_pobj:.o=.o.d)
cxx_dod   := $(cxx_dobj:.o=.o.d)

odb      := $(out_base)/odb
odb_so   := $(out_base)/odb.so
clean    := $(out_base)/.clean
install  := $(out_base)/.install

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
$(odb): $(cxx_dobj) $(cutl.l) | $(odb_so)
$(odb_so): $(cxx_pobj) $(cutl.l)

$(cxx_dobj) $(cxx_dod): cpp_options := -I$(src_root)
$(cxx_pobj) $(cxx_dobj) $(cxx_pod) $(cxx_dod): $(cutl.l.cpp-options)

genf := $(cli_tun:.cli=.hxx) $(cli_tun:.cli=.ixx) $(cli_tun:.cli=.cxx)
gen  := $(addprefix $(out_base)/,$(genf))

$(gen): cli := $(cli)
$(gen): cli_options :=  \
--generate-file-scanner \
--include-with-brackets \
--include-prefix odb    \
--guard-prefix ODB

$(call include-dep,$(cxx_pod))
$(call include-dep,$(cxx_dod))

# Alias for default target.
#
$(out_base)/: $(odb)

# Install.
#
$(install): $(odb)
	$(call install-exec,$<,$(install_bin_dir)/odb)
	$(call install-exec,$<.so,$(install_bin_dir)/odb.so)

# Clean.
#
$(clean):                             \
  $(odb).o.clean                      \
  $(addsuffix .cxx.clean,$(cxx_pobj)) \
  $(addsuffix .cxx.clean,$(cxx_dobj)) \
  $(addsuffix .cxx.clean,$(cxx_pod))  \
  $(addsuffix .cxx.clean,$(cxx_dod))
	$(call message,rm $$1,rm -f $$1,$(out_base)/odb.so)

# Generated .gitignore.
#
ifeq ($(out_base),$(src_base))
$(odb): | $(out_base)/.gitignore

$(out_base)/.gitignore: files := odb odb.so
$(clean): $(out_base)/.gitignore.clean

$(call include,$(bld_root)/git/gitignore.make)
endif

# Rules.
#
$(call include,$(bld_root)/install.make)
$(call include,$(cli_rules))
$(call include,$(bld_root)/cxx/cxx-d.make)
$(call include,$(bld_root)/cxx/cxx-o.make)
$(call include,$(bld_root)/cxx/o-e.make)

# Custom rules for the plugin.
#
ifdef cxx_gnu

$(cxx_pobj): cxx_pic_options := -fPIC

$(cxx_pobj) $(cxx_pod): cpp_options := -I$(src_root) \
-I$(shell $(cxx_gnu) -print-file-name=plugin)/include

$(odb_so):
	$(call message,ld $@,$(ld) \
$(cxx_extra_options) $(ld_options) $(cxx_ld_extra_options) -shared -o $@ \
$(foreach f,$^,$(if $(patsubst %.l,,$f),$f,$(call expand-l,$f))) $(cxx_extra_libs))

endif
