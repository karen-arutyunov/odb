// file      : odb/sqlite/connection.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <new>    // std::bad_alloc
#include <string>
#include <cassert>

#include <odb/details/lock.hxx>

#include <odb/sqlite/database.hxx>
#include <odb/sqlite/connection.hxx>
#include <odb/sqlite/statement.hxx>
#include <odb/sqlite/statement-cache.hxx>
#include <odb/sqlite/error.hxx>
#include <odb/sqlite/exceptions.hxx> // deadlock

#include <odb/sqlite/details/config.hxx> // LIBODB_SQLITE_HAVE_UNLOCK_NOTIFY

using namespace std;

extern "C" void
odb_sqlite_connection_unlock_callback (void**, int);

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
    connection (database_type& db, int extra_flags)
        : db_ (db), unlock_cond_ (unlock_mutex_), statements_ (0)
    {
      int f (db.flags () | extra_flags);
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

    inline void
    connection_unlock_callback (void** args, int n)
    {
      for (int i (0); i < n; ++i)
      {
        connection* c (static_cast<connection*> (args[i]));
        details::lock l (c->unlock_mutex_);
        c->unlocked_ = true;
        c->unlock_cond_.signal ();
      }
    }

    void connection::
    wait ()
    {
#ifdef LIBODB_SQLITE_HAVE_UNLOCK_NOTIFY
      unlocked_ = false;

      // unlock_notify() returns SQLITE_OK or SQLITE_LOCKED (deadlock).
      //
      int e (sqlite3_unlock_notify (handle_,
                                    &odb_sqlite_connection_unlock_callback,
                                    this));
      if (e == SQLITE_LOCKED)
        throw deadlock ();

      details::lock l (unlock_mutex_);

      while (!unlocked_)
        unlock_cond_.wait ();
#else
      translate_error (SQLITE_LOCKED, *this);
#endif
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

extern "C" void
odb_sqlite_connection_unlock_callback (void** args, int n)
{
  odb::sqlite::connection_unlock_callback (args, n);
}