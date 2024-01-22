# file      : makefile
# license   : GNU GPL v2; see accompanying LICENSE file

include $(dir $(lastword $(MAKEFILE_LIST)))build/bootstrap.make

dirs := odb

default  := $(out_base)/
dist     := $(out_base)/.dist
clean    := $(out_base)/.clean

$(default): $(addprefix $(out_base)/,$(addsuffix /,$(dirs)))

$(dist): export dirs := $(dirs)
$(dist): export docs := GPLv2 LICENSE README NEWS version.txt
$(dist): data_dist := INSTALL libodb-vc8.sln libodb-vc9.sln libodb-vc10.sln \
libodb-vc11.sln libodb-vc12.sln
$(dist): exec_dist := bootstrap
$(dist): export extra_dist := $(data_dist) $(exec_dist)
$(dist): export version = $(shell cat $(src_root)/version.txt)

$(dist): $(addprefix $(out_base)/,$(addsuffix /.dist,$(dirs)))
	$(call dist-data,$(docs) $(data_dist) libodb.pc.in)
	$(call dist-exec,$(exec_dist))
	$(call dist-dir,m4)
	$(call meta-automake)
	$(call meta-autoconf)

$(clean): $(addprefix $(out_base)/,$(addsuffix /.clean,$(dirs)))

$(call include,$(bld_root)/dist.make)
$(call include,$(bld_root)/meta/automake.make)
$(call include,$(bld_root)/meta/autoconf.make)

$(foreach d,$(dirs),$(call import,$(src_base)/$d/makefile))
