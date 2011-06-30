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
#include <odb/details/mutex.hxx>
#include <odb/details/condition.hxx>
#include <odb/details/shared-ptr.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/forward.hxx>
#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    class statement;
    class statement_cache;

    class LIBODB_SQLITE_EXPORT connection: public details::shared_base
    {
    public:
      typedef sqlite::statement_cache statement_cache_type;
      typedef sqlite::database database_type;

      virtual
      ~connection ();

      connection (database_type&, int extra_flags = 0);

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

      // Wait for the locks to be released via unlock notification. Can
      // be called after getting SQLITE_LOCKED_SHAREDCACHE.
      //
      void
      wait ();

    public:
      // Reset active and finalize uncached statements.
      //
      void
      clear ();

    private:
      connection (const connection&);
      connection& operator= (const connection&);

    private:
      database_type& db_;
      sqlite3* handle_;

      // Unlock notification machinery.
      //
    private:
      bool unlocked_;
      details::mutex unlock_mutex_;
      details::condition unlock_cond_;

      friend void
      connection_unlock_callback (void**, int);

      // Linked list of active and uncached statements currently associated
      // with this connection.
      //
    private:
      friend class statement;
      statement* statements_;

      std::auto_ptr<statement_cache_type> statement_cache_;
    };
  }
}

#include <odb/post.hxx>

#endif // ODB_SQLITE_CONNECTION_HXX