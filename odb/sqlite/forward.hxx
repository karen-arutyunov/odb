// file      : odb/sqlite/forward.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_FORWARD_HXX
#define ODB_SQLITE_FORWARD_HXX

#include <odb/pre.hxx>

#include <odb/forward.hxx>

namespace odb
{
  namespace sqlite
  {
    class database;
    class connection;
    typedef details::shared_ptr<connection> connection_ptr;
    class connection_factory;
    class transaction;
    class query;

    // Implementation details.
    //
    class binding;
    class select_statement;

    template <typename T>
    class object_statements;

    template <typename T>
    class container_statements;

    class query_params;
  }

  namespace details
  {
    template <>
    struct counter_type<sqlite::connection>
    {
      typedef shared_base counter;
    };

    template <>
    struct counter_type<sqlite::query_params>
    {
      typedef shared_base counter;
    };
  }
}

#include <odb/post.hxx>

#endif // ODB_SQLITE_FORWARD_HXX
