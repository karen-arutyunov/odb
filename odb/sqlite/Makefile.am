# file      : odb/sqlite/Makefile.am
# copyright : Copyright (c) 2009-2018 Code Synthesis Tools CC
# license   : GNU GPL v2; see accompanying LICENSE file

lib_LTLIBRARIES = libodb-sqlite.la
libodb_sqlite_la_SOURCES = __path__(sources)

odbsqliteincludedir = $(includedir)/odb/sqlite/
nobase_odbsqliteinclude_HEADERS = __path__(headers)
nobase_nodist_odbsqliteinclude_HEADERS = details/config.h

EXTRA_DIST = __file__(extra_dist)

AM_CPPFLAGS= -I'$(top_builddir)' -I'$(top_srcdir)' -DLIBODB_SQLITE_DYNAMIC_LIB
AM_LDFLAGS = -release __value__(interface_version) -no-undefined
