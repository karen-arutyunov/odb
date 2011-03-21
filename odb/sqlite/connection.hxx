// file      : odb/sqlite/connection.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_CONNECTION_HXX
#define ODB_SQLITE_CONNECTION_HXX

#include <odb/pre.hxx>

#include <sqlite3.h>

#include <memory> // std::auto_ptr

#include <odb/forward.hxx>
#include <odb/details/shared-ptr.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/forward.hxx>
#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    class statement_cache;

    class LIBODB_SQLITE_EXPORT connection: public details::shared_base
    {
    public:
      typedef sqlite::statement_cache statement_cache_type;
      typedef sqlite::database database_type;

      virtual
      ~connection ();

      connection (database_type&);

      database_type&
      database ()
      {
        return db_;
      }

    public:
      sqlite3*
      handle ()
      {
        return handle_;
      }

      statement_cache_type&
      statement_cache ()
      {
        return *statement_cache_;
      }

    private:
      connection (const connection&);
      connection& operator= (const connection&);

    private:
      database_type& db_;
      sqlite3* handle_;

      std::auto_ptr<statement_cache_type> statement_cache_;
    };
  }
}

#include <odb/post.hxx>

#endif // ODB_SQLITE_CONNECTION_HXX
