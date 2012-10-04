// file      : odb/sqlite/prepared-query.hxx
// copyright : Copyright (c) 2005-2012 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_PREPARED_QUERY_HXX
#define ODB_SQLITE_PREPARED_QUERY_HXX

#include <odb/pre.hxx>

#include <odb/prepared-query.hxx>
#include <odb/details/shared-ptr.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/query.hxx>
#include <odb/sqlite/statement.hxx>

#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    struct LIBODB_SQLITE_EXPORT prepared_query_impl: odb::prepared_query_impl
    {
      sqlite::query query;
      details::shared_ptr<select_statement> stmt;
    };
  }
}

#include <odb/post.hxx>

#endif // ODB_SQLITE_PREPARED_QUERY_HXX
