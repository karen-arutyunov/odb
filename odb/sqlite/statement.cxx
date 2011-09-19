// file      : odb/sqlite/statement.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cstring> // std::memcpy
#include <cassert>

#include <odb/exceptions.hxx> // object_not_persistent

#include <odb/sqlite/statement.hxx>
#include <odb/sqlite/connection.hxx>
#include <odb/sqlite/error.hxx>

using namespace std;

namespace odb
{
  namespace sqlite
  {
    // statement
    //

    statement::
    ~statement ()
    {
      finilize ();
    }

    void statement::
    init (const char* s, std::size_t n)
    {
      int e;
      sqlite3_stmt* stmt (0);
      while ((e = sqlite3_prepare_v2 (conn_.handle (),
                                      s,
                                      static_cast<int> (n),
                                      &stmt,
                                      0)) == SQLITE_LOCKED)
      {
        conn_.wait ();
      }

      if (e != SQLITE_OK)
        translate_error (e, conn_);

      stmt_.reset (stmt);

      active_ = false;
      cached_ = false;

      prev_ = 0;
      next_ = this;

      list_add (); // Add to the list because we are uncached.
    }

    void statement::
    bind_param (const bind* p, size_t n, size_t start_param)
    {
      int e (SQLITE_OK);
      start_param++; // SQLite parameters are counted from 1.

      for (size_t i (0); e == SQLITE_OK && i < n; ++i)
      {
        const bind& b (p[i]);
        int j (static_cast<int> (i + start_param));

        if (b.is_null != 0 && *b.is_null)
        {
          e = sqlite3_bind_null (stmt_, j);
          continue;
        }

        switch (b.type)
        {
        case bind::integer:
          {
            long long v (*static_cast<long long*> (b.buffer));
            e = sqlite3_bind_int64 (stmt_, j, static_cast<sqlite3_int64> (v));
            break;
          }
        case bind::real:
          {
            double v (*static_cast<double*> (b.buffer));
            e = sqlite3_bind_double (stmt_, j, v);
            break;
          }
        case bind::text:
          {
            e = sqlite3_bind_text (stmt_,
                                   j,
                                   static_cast<const char*> (b.buffer),
                                   static_cast<int> (*b.size),
                                   SQLITE_STATIC);
            break;
          }
        case bind::blob:
          {
            e = sqlite3_bind_blob (stmt_,
                                   j,
                                   b.buffer,
                                   static_cast<int> (*b.size),
                                   SQLITE_STATIC);
            break;
          }
        }
      }

      if (e != SQLITE_OK)
        translate_error (e, conn_);
    }

    bool statement::
    bind_result (const bind* p, size_t n, bool truncated)
    {
      // Make sure that the number of columns in the result returned by
      // the database matches the number that we expect. A common cause
      // of this assertion is a native view with a number of data members
      // not matching the number of columns in the SELECT-list.
      //
      assert (static_cast<size_t> (sqlite3_data_count (stmt_)) == n);

      bool r (true);

      for (size_t i (0); i < n; ++i)
      {
        const bind& b (p[i]);
        int j (static_cast<int> (i));

        if (truncated && (b.truncated == 0 || !*b.truncated))
          continue;

        if (b.truncated != 0)
          *b.truncated = false;

        // Check for NULL unless we are reloading a truncated result.
        //
        if (!truncated)
        {
          *b.is_null = sqlite3_column_type (stmt_, j) == SQLITE_NULL;

          if (*b.is_null)
            continue;
        }

        switch (b.type)
        {
        case bind::integer:
          {
            *static_cast<long long*> (b.buffer) =
              static_cast<long long> (sqlite3_column_int64 (stmt_, j));
            break;
          }
        case bind::real:
          {
            *static_cast<double*> (b.buffer) =
              sqlite3_column_double (stmt_, j);
            break;
          }
        case bind::text:
        case bind::blob:
          {
            *b.size = static_cast<size_t> (sqlite3_column_bytes (stmt_, j));

            if (*b.size > b.capacity)
            {
              if (b.truncated != 0)
                *b.truncated = true;

              r = false;
              continue;
            }

            const void* d (b.type == bind::text
                           ? sqlite3_column_text (stmt_, j)
                           : sqlite3_column_blob (stmt_, j));

            memcpy (b.buffer, d, *b.size);
            break;
          }
        }
      }

      return r;
    }

    // simple_statement
    //

    simple_statement::
    simple_statement (connection& conn, const string& s)
        : statement (conn, s),
          result_set_ (stmt_ ? sqlite3_column_count (stmt_) != 0: false)
    {
    }

    simple_statement::
    simple_statement (connection& conn, const char* s, std::size_t n)
        : statement (conn, s, n),
          result_set_ (stmt_ ? sqlite3_column_count (stmt_) != 0: false)
    {
    }

    unsigned long long simple_statement::
    execute ()
    {
      if (stmt_ == 0) // Empty statement or comment.
        return 0;

      unsigned long long r (0);

      // Only the first call to sqlite3_step() can return SQLITE_LOCKED.
      //
      int e;
      sqlite3* h (conn_.handle ());
      while ((e = sqlite3_step (stmt_)) == SQLITE_LOCKED)
      {
        if (sqlite3_extended_errcode (h) != SQLITE_LOCKED_SHAREDCACHE)
          break;

        sqlite3_reset (stmt_);
        conn_.wait ();
      }

      for (; e == SQLITE_ROW; e = sqlite3_step (stmt_))
        r++;

      sqlite3_reset (stmt_);

      if (e != SQLITE_DONE)
        translate_error (e, conn_);

      if (!result_set_)
        r = static_cast<unsigned long long> (
          sqlite3_changes (conn_.handle ()));

      return r;
    }

    // select_statement
    //

    select_statement::
    select_statement (connection& conn,
                      const string& s,
                      binding& cond,
                      binding& data)
        : statement (conn, s), cond_ (&cond), data_ (data)
    {
    }

    select_statement::
    select_statement (connection& conn,
                      const string& s,
                      binding& data)
        : statement (conn, s), cond_ (0), data_ (data)
    {
    }

    void select_statement::
    execute ()
    {
      if (active ())
        reset ();

      done_ = false;

      if (cond_ != 0)
        bind_param (cond_->bind, cond_->count);

      active (true);
    }

    void select_statement::
    free_result ()
    {
      reset ();
      done_ = true;
    }

    bool select_statement::
    next ()
    {
      if (!done_)
      {
        int e;
        sqlite3* h (conn_.handle ());
        while ((e = sqlite3_step (stmt_)) == SQLITE_LOCKED)
        {
          if (sqlite3_extended_errcode (h) != SQLITE_LOCKED_SHAREDCACHE)
            break;

          sqlite3_reset (stmt_);
          conn_.wait ();
        }

        if (e != SQLITE_ROW)
        {
          reset ();
          done_ = true;

          if (e != SQLITE_DONE)
            translate_error (e, conn_);
        }
      }

      return !done_;
    }

    select_statement::result select_statement::
    load ()
    {
      if (done_)
        return no_data;

      return bind_result (data_.bind, data_.count) ? success : truncated;
    }

    void select_statement::
    reload ()
    {
      assert (!done_);

      if (!bind_result (data_.bind, data_.count, true))
        assert (false);
    }

    // insert_statement
    //

    insert_statement::
    insert_statement (connection& conn, const string& s, binding& data)
        : statement (conn, s), data_ (data)
    {
    }

    bool insert_statement::
    execute ()
    {
      bind_param (data_.bind, data_.count);

      int e;
      sqlite3* h (conn_.handle ());
      while ((e = sqlite3_step (stmt_)) == SQLITE_LOCKED)
      {
        if (sqlite3_extended_errcode (h) != SQLITE_LOCKED_SHAREDCACHE)
          break;

        sqlite3_reset (stmt_);
        conn_.wait ();
      }

      sqlite3_reset (stmt_);

      if (e != SQLITE_DONE)
      {
        // SQLITE_CONSTRAINT error code covers more than just a duplicate
        // primary key. Unfortunately, there is nothing more precise that
        // we can use (even sqlite3_errmsg() returns generic "constraint
        // failed").
        //
        if (e == SQLITE_CONSTRAINT)
          return false;
        else
          translate_error (e, conn_);
      }

      return true;
    }

    unsigned long long insert_statement::
    id ()
    {
      return static_cast<unsigned long long> (
        sqlite3_last_insert_rowid (conn_.handle ()));
    }

    // update_statement
    //

    update_statement::
    update_statement (connection& conn,
                      const string& s,
                      binding& cond,
                      binding& data)
        : statement (conn, s), cond_ (cond), data_ (data)
    {
    }

    void update_statement::
    execute ()
    {
      bind_param (data_.bind, data_.count);
      bind_param (cond_.bind, cond_.count, data_.count);

      int e;
      sqlite3* h (conn_.handle ());
      while ((e = sqlite3_step (stmt_)) == SQLITE_LOCKED)
      {
        if (sqlite3_extended_errcode (h) != SQLITE_LOCKED_SHAREDCACHE)
          break;

        sqlite3_reset (stmt_);
        conn_.wait ();
      }

      sqlite3_reset (stmt_);

      if (e != SQLITE_DONE)
        translate_error (e, conn_);

      if (sqlite3_changes (conn_.handle ()) == 0)
        throw object_not_persistent ();
    }

    // delete_statement
    //

    delete_statement::
    delete_statement (connection& conn, const string& s, binding& cond)
        : statement (conn, s), cond_ (cond)
    {
    }

    unsigned long long delete_statement::
    execute ()
    {
      bind_param (cond_.bind, cond_.count);

      int e;
      sqlite3* h (conn_.handle ());
      while ((e = sqlite3_step (stmt_)) == SQLITE_LOCKED)
      {
        if (sqlite3_extended_errcode (h) != SQLITE_LOCKED_SHAREDCACHE)
          break;

        sqlite3_reset (stmt_);
        conn_.wait ();
      }

      sqlite3_reset (stmt_);

      if (e != SQLITE_DONE)
        translate_error (e, conn_);

      return static_cast<unsigned long long> (
        sqlite3_changes (conn_.handle ()));
    }
  }
}
