// file      : odb/sqlite/statement.cxx
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/tracer.hxx>
#include <odb/exceptions.hxx> // object_not_persistent

#include <odb/sqlite/database.hxx>
#include <odb/sqlite/statement.hxx>
#include <odb/sqlite/connection.hxx>
#include <odb/sqlite/error.hxx>

#include <odb/sqlite/details/config.hxx> // LIBODB_SQLITE_HAVE_UNLOCK_NOTIFY
                                         // LIBODB_SQLITE_HAVE_COLUMN_METADATA
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
      if (stmt_ != 0)
      {
        {
          odb::tracer* t;
          if ((t = conn_.main_connection ().transaction_tracer ()) ||
              (t = conn_.tracer ()) ||
              (t = conn_.database ().tracer ()))
            t->deallocate (conn_, *this);
        }

        if (next_ != this)
          list_remove ();

        stmt_.reset ();
      }
    }

    void statement::
    clear ()
    {
      reset ();
    }

    void statement::
    init (const char* text,
          std::size_t text_size,
          statement_kind sk,
          const binding* proc,
          bool optimize)
    {
      active_ = false;

      string tmp1;
      if (proc != 0)
      {
        switch (sk)
        {
        case statement_select:
          process_select (tmp1,
                          text,
                          &proc->bind->buffer, proc->count, sizeof (bind),
                          '"', '"',
                          optimize);
          break;
        case statement_insert:
          process_insert (tmp1,
                          text,
                          &proc->bind->buffer, proc->count, sizeof (bind),
                          '?',
                          '$');
          break;
        case statement_update:
          process_update (tmp1,
                          text,
                          &proc->bind->buffer, proc->count, sizeof (bind),
                          '?',
                          '$');
          break;
        case statement_delete:
        case statement_generic:
          assert (false);
        }

        text = tmp1.c_str ();
        text_size = tmp1.size ();
      }

      string tmp2;
      if (conn_.statement_translator_ != 0)
      {
        conn_.statement_translator_ (tmp2, text, text_size, conn_);

        if (!tmp2.empty ())
        {
          text = tmp2.c_str ();
          text_size = tmp2.size ();
        }
      }

#if SQLITE_VERSION_NUMBER < 3005003
      text_.assign (text, text_size);
#endif

      // Empty statement.
      //
      if (*text == '\0')
        return;

      {
        odb::tracer* t;
        if ((t = conn_.main_connection ().transaction_tracer ()) ||
            (t = conn_.tracer ()) ||
            (t = conn_.database ().tracer ()))
        {
          // Temporarily store the statement text in prev_ so that
          // text() which may be called by the tracer can access it.
          // Dirty but efficient.
          //
#if SQLITE_VERSION_NUMBER >= 3005003
          prev_ = reinterpret_cast<active_object*> (const_cast<char*> (text));
#endif
          t->prepare (conn_, *this);
#if SQLITE_VERSION_NUMBER >= 3005003
          prev_ = 0;
#endif
        }
      }

      int e;
      sqlite3_stmt* stmt (0);

      // sqlite3_prepare_v2() is only available since SQLite 3.3.9
      // but is buggy until 3.3.11.
      //
#if SQLITE_VERSION_NUMBER >= 3003011
      while ((e = sqlite3_prepare_v2 (conn_.handle (),
                                      text,
                                      static_cast<int> (text_size),
                                      &stmt,
                                      0)) == SQLITE_LOCKED)
      {
        conn_.wait ();
      }
#else
      e = sqlite3_prepare (conn_.handle (),
                           text,
                           static_cast<int> (text_size),
                           &stmt,
                           0);
#endif

      if (e != SQLITE_OK)
        translate_error (e, conn_);

      stmt_.reset (stmt);
    }

    const char* statement::
    text () const
    {
      // sqlite3_sql() is only available since 3.5.3.
      //
#if SQLITE_VERSION_NUMBER >= 3005003
      if (stmt_ == 0)
        // See init() above for details on what's going on here.
        //
        return prev_ != 0 ? reinterpret_cast<const char*> (prev_) : "";
      else
        return sqlite3_sql (stmt_);
#else
      return text_.c_str ();
#endif
    }

    bool statement::
    bind_param (const bind* p, size_t n)
    {
      int e (SQLITE_OK);
      bool r (false);

      // SQLite parameters are counted from 1.
      //
      for (size_t i (0), j (1); e == SQLITE_OK && i < n; ++i)
      {
        const bind& b (p[i]);

        if (b.buffer == 0) // Skip NULL entries.
          continue;

        int c (static_cast<int> (j++));

        if (b.is_null != 0 && *b.is_null)
        {
          e = sqlite3_bind_null (stmt_, c);
          continue;
        }

        switch (b.type)
        {
        case bind::integer:
          {
            long long v (*static_cast<long long*> (b.buffer));

            e = sqlite3_bind_int64 (stmt_,
                                    c,
            // Prior to 3.5.0, sqlite3_int64 was called sqlite_int64.
#if SQLITE_VERSION_NUMBER >= 3005000
                                    static_cast<sqlite3_int64> (v)
#else
                                    static_cast<sqlite_int64> (v)
#endif
            );
            break;
          }
        case bind::real:
          {
            double v (*static_cast<double*> (b.buffer));
            e = sqlite3_bind_double (stmt_, c, v);
            break;
          }
        case bind::text:
          {
            e = sqlite3_bind_text (stmt_,
                                   c,
                                   static_cast<const char*> (b.buffer),
                                   static_cast<int> (*b.size),
                                   SQLITE_STATIC);
            break;
          }
        case bind::text16:
          {
            e = sqlite3_bind_text16 (stmt_,
                                     c,
                                     b.buffer,
                                     static_cast<int> (*b.size),
                                     SQLITE_STATIC);
            break;
          }
        case bind::blob:
          {
            e = sqlite3_bind_blob (stmt_,
                                   c,
                                   b.buffer,
                                   static_cast<int> (*b.size),
                                   SQLITE_STATIC);
            break;
          }
        case bind::stream:
          {
#if SQLITE_VERSION_NUMBER >= 3004000
            e = sqlite3_bind_zeroblob (stmt_,
                                       c,
                                       static_cast<int> (*b.size));
            r = true;
#else
            assert (false);
#endif
            break;
          }
        }
      }

      if (e != SQLITE_OK)
        translate_error (e, conn_);

      return r;
    }

    bool statement::
    bind_result (const bind* p, size_t count, bool truncated)
    {
      bool r (true);
      int col_count (sqlite3_data_count (stmt_));

      int col (0);
      for (size_t i (0); i != count && col != col_count; ++i)
      {
        const bind& b (p[i]);

        if (b.buffer == 0) // Skip NULL entries.
          continue;

        int c (col++);

        if (b.type == bind::stream)
          col++; // Skip ROWID value that follows.

        if (truncated && (b.truncated == 0 || !*b.truncated))
          continue;

        if (b.truncated != 0)
          *b.truncated = false;

        // Check for NULL unless we are reloading a truncated result.
        //
        if (!truncated)
        {
          *b.is_null = sqlite3_column_type (stmt_, c) == SQLITE_NULL;

          if (*b.is_null)
            continue;
        }

        switch (b.type)
        {
        case bind::integer:
          {
            *static_cast<long long*> (b.buffer) =
              static_cast<long long> (sqlite3_column_int64 (stmt_, c));
            break;
          }
        case bind::real:
          {
            *static_cast<double*> (b.buffer) =
              sqlite3_column_double (stmt_, c);
            break;
          }
        case bind::text:
        case bind::text16:
        case bind::blob:
          {
            // SQLite documentation recommends that we first call *_text(),
            // *_text16(), or *_blob() function in order to force the type
            // conversion, if any.
            //
            const void* d;

            if (b.type != bind::text16)
            {
              d = b.type == bind::text
                ? sqlite3_column_text (stmt_, c)
                : sqlite3_column_blob (stmt_, c);
              *b.size = static_cast<size_t> (sqlite3_column_bytes (stmt_, c));
            }
            else
            {
              d = sqlite3_column_text16 (stmt_, c);
              *b.size = static_cast<size_t> (
                sqlite3_column_bytes16 (stmt_, c));
            }

            if (*b.size > b.capacity)
            {
              if (b.truncated != 0)
                *b.truncated = true;

              r = false;
              continue;
            }

            memcpy (b.buffer, d, *b.size);
            break;
          }
        case bind::stream:
          {
            stream_buffers& sb (*static_cast<stream_buffers*> (b.buffer));

            // SQLite documentation states that these are valid until the
            // statement is finalized (or reprepared). For our case, we
            // only need it to stay alive until we call set_value() which
            // we do while executing the statement (i.e., we don't copy
            // images for later processing).
            //
#ifdef LIBODB_SQLITE_HAVE_COLUMN_METADATA
            sb.db.in = sqlite3_column_database_name (stmt_, c);
            sb.table.in = sqlite3_column_table_name (stmt_, c);
            sb.column.in = sqlite3_column_origin_name (stmt_, c);
#else
            assert (false);
#endif

            // The ROWID comes in the following column.
            //
            sb.rowid.in = static_cast<long long> (
              sqlite3_column_int64 (stmt_, c + 1));

            break;
          }
        }
      }

      // Make sure that the number of columns in the result returned by
      // the database matches the number that we expect. A common cause
      // of this assertion is a native view with a number of data members
      // not matching the number of columns in the SELECT-list.
      //
      assert (col == col_count);

      return r;
    }

    void statement::
    stream_param (const bind* p, size_t n, const stream_data& d)
    {
      // Code similar to bind_param().
      //
      for (size_t i (0), j (1); i < n; ++i)
      {
        const bind& b (p[i]);

        if (b.buffer == 0) // Skip NULL entries.
          continue;

        int c (static_cast<int> (j++));

        if ((b.is_null != 0 && *b.is_null) || b.type != bind::stream)
          continue;

        // Get column name.
        //
        const char* col (sqlite3_bind_parameter_name (stmt_, c));
        assert (col != 0); // Statement doesn't contain column name.

        stream_buffers& sb (*static_cast<stream_buffers*> (b.buffer));

        *sb.db.out = d.db;
        *sb.table.out = d.table;
        *sb.column.out = col + 1; // Skip '$'.
        *sb.rowid.out = d.rowid;
      }
    }

    inline void
    update_hook (void* v, const char* db, const char* table, long long rowid)
    {
      statement::stream_data& d (*static_cast<statement::stream_data*> (v));
      d.db = db;
      d.table = table;
      d.rowid = rowid;
    }

    extern "C" void
    odb_sqlite_update_hook (void* v,
                            int,
                            const char* db,
                            const char* table,
#if SQLITE_VERSION_NUMBER >= 3005000
                                    sqlite3_int64 rowid
#else
                                    sqlite_int64 rowid
#endif
    )
    {
      update_hook (v, db, table, static_cast<long long> (rowid));
    }

    // generic_statement
    //

    generic_statement::
    generic_statement (connection_type& conn, const string& text)
        : statement (conn,
                     text, statement_generic,
                     0, false),
          result_set_ (stmt_ ? sqlite3_column_count (stmt_) != 0: false)
    {
    }

    generic_statement::
    generic_statement (connection_type& conn, const char* text)
        : statement (conn,
                     text, statement_generic,
                     0, false),
          result_set_ (stmt_ ? sqlite3_column_count (stmt_) != 0: false)
    {
    }

    generic_statement::
    generic_statement (connection_type& conn,
                       const char* text,
                       size_t text_size)
        : statement (conn,
                     text, text_size, statement_generic,
                     0, false),
          result_set_ (stmt_ ? sqlite3_column_count (stmt_) != 0: false)
    {
    }

    unsigned long long generic_statement::
    execute ()
    {
      if (stmt_ == 0) // Empty statement or comment.
        return 0;

      {
        odb::tracer* t;
        if ((t = conn_.main_connection ().transaction_tracer ()) ||
            (t = conn_.tracer ()) ||
            (t = conn_.database ().tracer ()))
          t->execute (conn_, *this);
      }

      unsigned long long r (0);

      int e;
      sqlite3* h (conn_.handle ());

#ifdef LIBODB_SQLITE_HAVE_UNLOCK_NOTIFY
      // Only the first call to sqlite3_step() can return SQLITE_LOCKED.
      //
      while ((e = sqlite3_step (stmt_)) == SQLITE_LOCKED)
      {
        if (sqlite3_extended_errcode (h) != SQLITE_LOCKED_SHAREDCACHE)
          break;

        sqlite3_reset (stmt_);
        conn_.wait ();
      }
#else
      e = sqlite3_step (stmt_);
#endif

      for (; e == SQLITE_ROW; e = sqlite3_step (stmt_))
        r++;

      // sqlite3_step() will return a detailed error code only if we used
      // sqlite3_prepare_v2(). Otherwise, sqlite3_reset() returns the
      // error.
      //
#if SQLITE_VERSION_NUMBER >= 3003011
      sqlite3_reset (stmt_);

      if (e != SQLITE_DONE)
        translate_error (e, conn_);
#else
      e = sqlite3_reset (stmt_);

      if (e != SQLITE_OK)
      {
        // If the schema has changed, try to re-prepare and re-execute the
        // statement. That's what newer versions of SQLite do automatically.
        //
        if (e == SQLITE_SCHEMA)
        {
          sqlite3_stmt* stmt (0);
          e = sqlite3_prepare (h,
                               text_.c_str (),
                               static_cast<int> (text_.size () + 1),
                               &stmt,
                               0);

          if (e != SQLITE_OK)
            translate_error (e, conn_);

          stmt_.reset (stmt);
          return execute (); // Try again by recursively calling ourselves.
        }
        else
          translate_error (e, conn_);
      }
#endif

      if (!result_set_)
        r = static_cast<unsigned long long> (sqlite3_changes (h));

      return r;
    }

    // select_statement
    //

    select_statement::
    select_statement (connection_type& conn,
                      const string& text,
                      bool process,
                      bool optimize,
                      binding& param,
                      binding& result)
        : statement (conn,
                     text, statement_select,
                     (process ? &result : 0), optimize),
          param_ (&param),
          result_ (result)
    {
    }

    select_statement::
    select_statement (connection_type& conn,
                      const char* text,
                      bool process,
                      bool optimize,
                      binding& param,
                      binding& result)
        : statement (conn,
                     text, statement_select,
                     (process ? &result : 0), optimize),
          param_ (&param),
          result_ (result)
    {
    }

    select_statement::
    select_statement (connection_type& conn,
                      const string& text,
                      bool process,
                      bool optimize,
                      binding& result)
        : statement (conn,
                     text, statement_select,
                     (process ? &result : 0), optimize),
          param_ (0),
          result_ (result)
    {
    }

    select_statement::
    select_statement (connection_type& conn,
                      const char* text,
                      bool process,
                      bool optimize,
                      binding& result)
        : statement (conn,
                     text, statement_select,
                     (process ? &result : 0), optimize),
          param_ (0),
          result_ (result)
    {
    }

    void select_statement::
    execute ()
    {
      if (active ())
        reset ();

      {
        odb::tracer* t;
        if ((t = conn_.main_connection ().transaction_tracer ()) ||
            (t = conn_.tracer ()) ||
            (t = conn_.database ().tracer ()))
          t->execute (conn_, *this);
      }

      done_ = false;

      if (param_ != 0)
        bind_param (param_->bind, param_->count);

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

#ifdef LIBODB_SQLITE_HAVE_UNLOCK_NOTIFY
        sqlite3* h (conn_.handle ());
        while ((e = sqlite3_step (stmt_)) == SQLITE_LOCKED)
        {
          if (sqlite3_extended_errcode (h) != SQLITE_LOCKED_SHAREDCACHE)
            break;

          sqlite3_reset (stmt_);
          conn_.wait ();
        }
#else
        e = sqlite3_step (stmt_);
#endif

        if (e != SQLITE_ROW)
        {
          done_ = true;

          // sqlite3_step() will return a detailed error code only if we used
          // sqlite3_prepare_v2(). Otherwise, sqlite3_reset() returns the
          // error.
          //
#if SQLITE_VERSION_NUMBER >= 3003011
          reset ();

          if (e != SQLITE_DONE)
#else
          e = reset ();

          if (e != SQLITE_OK)
#endif
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

      return bind_result (result_.bind, result_.count) ? success : truncated;
    }

    void select_statement::
    reload ()
    {
      assert (!done_);

      if (!bind_result (result_.bind, result_.count, true))
        assert (false);
    }

    // insert_statement
    //

    insert_statement::
    insert_statement (connection_type& conn,
                      const string& text,
                      bool process,
                      binding& param,
                      binding* returning)
        : statement (conn,
                     text, statement_insert,
                     (process ? &param : 0), false),
          param_ (param),
          returning_ (returning)
    {
    }

    insert_statement::
    insert_statement (connection_type& conn,
                      const char* text,
                      bool process,
                      binding& param,
                      binding* returning)
        : statement (conn,
                     text, statement_insert,
                     (process ? &param : 0), false),
          param_ (param),
          returning_ (returning)
    {
    }

    bool insert_statement::
    execute ()
    {
      {
        odb::tracer* t;
        if ((t = conn_.main_connection ().transaction_tracer ()) ||
            (t = conn_.tracer ()) ||
            (t = conn_.database ().tracer ()))
          t->execute (conn_, *this);
      }

      sqlite3* h (conn_.handle ());
      bool stream (bind_param (param_.bind, param_.count));

      stream_data sd;
      if (stream)
        sqlite3_update_hook (h, &odb_sqlite_update_hook, &sd);

      int e;

#ifdef LIBODB_SQLITE_HAVE_UNLOCK_NOTIFY
      while ((e = sqlite3_step (stmt_)) == SQLITE_LOCKED)
      {
        if (sqlite3_extended_errcode (h) != SQLITE_LOCKED_SHAREDCACHE)
          break;

        sqlite3_reset (stmt_);
        conn_.wait ();
      }
#else
      e = sqlite3_step (stmt_);
#endif

      if (stream)
        sqlite3_update_hook (h, 0, 0); // Clear the hook.

      // sqlite3_step() will return a detailed error code only if we used
      // sqlite3_prepare_v2(). Otherwise, sqlite3_reset() returns the
      // error.
      //
#if SQLITE_VERSION_NUMBER >= 3003011
      sqlite3_reset (stmt_);

      if (e != SQLITE_DONE)
#else
      e = sqlite3_reset (stmt_);

      if (e != SQLITE_OK)
#endif
      {
        // SQLITE_CONSTRAINT error code covers more than just a duplicate
        // primary key. Unfortunately, there is nothing more precise that
        // we can use (even sqlite3_errmsg() returns generic "constraint
        // failed"). But an auto-assigned object id should never cause a
        // duplicate primary key.
        //
        if (returning_ == 0 && e == SQLITE_CONSTRAINT)
          return false;
        else
          translate_error (e, conn_);
      }

      // Stream parameters, if any.
      //
      if (stream)
        stream_param (param_.bind, param_.count, sd);

      if (returning_ != 0)
      {
        bind& b (returning_->bind[0]);

        *b.is_null = false;
        *static_cast<long long*> (b.buffer) =
          static_cast<long long> (
            sqlite3_last_insert_rowid (h));
      }

      return true;
    }

    // update_statement
    //

    update_statement::
    update_statement (connection_type& conn,
                      const string& text,
                      bool process,
                      binding& param)
        : statement (conn,
                     text, statement_update,
                     (process ? &param : 0), false),
          param_ (param)
    {
    }

    update_statement::
    update_statement (connection_type& conn,
                      const char* text,
                      bool process,
                      binding& param)
        : statement (conn,
                     text, statement_update,
                     (process ? &param : 0), false),
          param_ (param)
    {
    }

    unsigned long long update_statement::
    execute ()
    {
      {
        odb::tracer* t;
        if ((t = conn_.main_connection ().transaction_tracer ()) ||
            (t = conn_.tracer ()) ||
            (t = conn_.database ().tracer ()))
          t->execute (conn_, *this);
      }

      sqlite3* h (conn_.handle ());
      bool stream (bind_param (param_.bind, param_.count));

      stream_data sd;
      if (stream)
        sqlite3_update_hook (h, &odb_sqlite_update_hook, &sd);

      int e;

#ifdef LIBODB_SQLITE_HAVE_UNLOCK_NOTIFY
      while ((e = sqlite3_step (stmt_)) == SQLITE_LOCKED)
      {
        if (sqlite3_extended_errcode (h) != SQLITE_LOCKED_SHAREDCACHE)
          break;

        sqlite3_reset (stmt_);
        conn_.wait ();
      }
#else
      e = sqlite3_step (stmt_);
#endif

      if (stream)
        sqlite3_update_hook (h, 0, 0); // Clear the hook.

      // sqlite3_step() will return a detailed error code only if we used
      // sqlite3_prepare_v2(). Otherwise, sqlite3_reset() returns the
      // error.
      //
#if SQLITE_VERSION_NUMBER >= 3003011
      sqlite3_reset (stmt_);

      if (e != SQLITE_DONE)
#else
      e = sqlite3_reset (stmt_);

      if (e != SQLITE_OK)
#endif
        translate_error (e, conn_);

      int r (sqlite3_changes (h));

      // Stream parameters, if any.
      //
      if (stream && r != 0)
        stream_param (param_.bind, param_.count, sd);

      return static_cast<unsigned long long> (r);
    }

    // delete_statement
    //

    delete_statement::
    delete_statement (connection_type& conn,
                      const string& text,
                      binding& param)
        : statement (conn,
                     text, statement_delete,
                     0, false),
          param_ (param)
    {
    }

    delete_statement::
    delete_statement (connection_type& conn,
                      const char* text,
                      binding& param)
        : statement (conn,
                     text, statement_delete,
                     0, false),
          param_ (param)
    {
    }

    unsigned long long delete_statement::
    execute ()
    {
      {
        odb::tracer* t;
        if ((t = conn_.main_connection ().transaction_tracer ()) ||
            (t = conn_.tracer ()) ||
            (t = conn_.database ().tracer ()))
          t->execute (conn_, *this);
      }

      bind_param (param_.bind, param_.count);

      int e;
      sqlite3* h (conn_.handle ());

#ifdef LIBODB_SQLITE_HAVE_UNLOCK_NOTIFY
      while ((e = sqlite3_step (stmt_)) == SQLITE_LOCKED)
      {
        if (sqlite3_extended_errcode (h) != SQLITE_LOCKED_SHAREDCACHE)
          break;

        sqlite3_reset (stmt_);
        conn_.wait ();
      }
#else
      e = sqlite3_step (stmt_);
#endif

      // sqlite3_step() will return a detailed error code only if we used
      // sqlite3_prepare_v2(). Otherwise, sqlite3_reset() returns the
      // error.
      //
#if SQLITE_VERSION_NUMBER >= 3003011
      sqlite3_reset (stmt_);

      if (e != SQLITE_DONE)
#else
      e = sqlite3_reset (stmt_);

      if (e != SQLITE_OK)
#endif
        translate_error (e, conn_);

      return static_cast<unsigned long long> (sqlite3_changes (h));
    }
  }
}
