// file      : odb/sqlite/statement.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

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
    statement (connection& conn, const string& s)
        : conn_ (conn)
    {
      if (int e = sqlite3_prepare_v2 (
            conn_.handle (),
            s.c_str (),
            static_cast<int> (s.size () + 1),
            &stmt_,
            0))
      {
        translate_error (e, conn_);
      }
    }

    statement::
    statement (connection& conn, const char* s, std::size_t n)
        : conn_ (conn)
    {
      if (int e = sqlite3_prepare_v2 (
            conn_.handle (),
            s,
            static_cast<int> (n),
            &stmt_,
            0))
      {
        translate_error (e, conn_);
      }
    }



    statement::
    ~statement ()
    {
      sqlite3_finalize (stmt_);
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

      if (int e = sqlite3_reset (stmt_))
        translate_error (e, conn_);

      unsigned long long r (0);

      int e;
      for (e = sqlite3_step (stmt_); e == SQLITE_ROW; e = sqlite3_step (stmt_))
        r++;

      if (e != SQLITE_DONE)
        translate_error (e, conn_);

      if (!result_set_)
        r = static_cast<unsigned long long> (
          sqlite3_changes (conn_.handle ()));

      return r;
    }
  }
}
