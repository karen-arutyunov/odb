// file      : odb/sqlite/connection.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <new>    // std::bad_alloc
#include <string>
#include <cassert>

#include <odb/sqlite/database.hxx>
#include <odb/sqlite/connection.hxx>
#include <odb/sqlite/statement.hxx>
#include <odb/sqlite/statement-cache.hxx>
#include <odb/sqlite/error.hxx>

using namespace std;

namespace odb
{
  namespace sqlite
  {
    connection::
    ~connection ()
    {
      statement_cache_.reset (); // Free prepared statements.

      if (sqlite3_close (handle_) == SQLITE_BUSY)
        assert (false); // Connection has outstanding prepared statements.
    }

    connection::
    connection (database_type& db)
        : db_ (db), statements_ (0)
    {
      int f (db.flags ());
      const string& n (db.name ());

      // If we are opening a temporary database, then add the create flag.
      //
      if (n.empty () || n == ":memory:")
        f |= SQLITE_OPEN_CREATE;

      // A connection can only be used by a single thread at a time. So
      // disable locking in SQLite unless explicitly requested.
      //
      if ((f & SQLITE_OPEN_FULLMUTEX) == 0)
        f |= SQLITE_OPEN_NOMUTEX;

      if (int e = sqlite3_open_v2 (n.c_str (), &handle_, f, 0))
      {
        if (handle_ == 0)
          throw bad_alloc ();

        translate_error (e, *this);
      }

      statement_cache_.reset (new statement_cache_type (*this));
    }

    void connection::
    clear ()
    {
      // The current first statement will remove itself from the list
      // and make the second statement (if any) the new first.
      //
      while (statement* s = statements_)
      {
        if (!s->cached ())
          s->finilize ();
        else if (s->active ())
          s->reset ();
        else
          assert (false); // Statement is neither active nor unached.
      }
    }
  }
}
