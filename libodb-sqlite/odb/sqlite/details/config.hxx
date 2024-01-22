// file      : odb/sqlite/details/config.hxx
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_DETAILS_CONFIG_HXX
#define ODB_SQLITE_DETAILS_CONFIG_HXX

// no pre

#ifdef ODB_COMPILER
#  error libodb-sqlite header included in odb-compiled header
#elif !defined(LIBODB_SQLITE_BUILD2)
#  ifdef _MSC_VER
#    include <odb/sqlite/details/config-vc.h>
#  else
#    include <odb/sqlite/details/config.h>
#  endif
#endif

// no post

#endif // ODB_SQLITE_DETAILS_CONFIG_HXX
