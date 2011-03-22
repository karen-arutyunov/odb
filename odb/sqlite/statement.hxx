// file      : odb/sqlite/statement.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_STATEMENT_HXX
#define ODB_SQLITE_STATEMENT_HXX

#include <odb/pre.hxx>

#include <sqlite3.h>

#include <string>
#include <cstddef>  // std::size_t

#include <odb/forward.hxx>
#include <odb/details/shared-ptr.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/binding.hxx>
#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    class connection;

    class LIBODB_SQLITE_EXPORT statement: public details::shared_base
    {
    public:
      virtual
      ~statement () = 0;

    protected:
      statement (connection&, const std::string& statement);
      statement (connection&, const char* statement, std::size_t n);

      void
      bind_param (const bind*, std::size_t count, std::size_t start_param = 0);

      // Extract row columns into the bound buffers. If the truncated
      // argument is true, then only truncated columns are extracted.
      // Return true if all the data was extracted successfully and
      // false if one or more columns were truncated.
      //
      bool
      bind_result (const bind*, std::size_t count, bool truncated = false);

    protected:
      connection& conn_;
      sqlite3_stmt* stmt_;
    };

    class LIBODB_SQLITE_EXPORT simple_statement: public statement
    {
    public:
      simple_statement (connection&, const std::string& statement);
      simple_statement (connection&, const char* statement, std::size_t n);

      unsigned long long
      execute ();

    private:
      simple_statement (const simple_statement&);
      simple_statement& operator= (const simple_statement&);

    private:
      bool result_set_;
    };

    class LIBODB_SQLITE_EXPORT select_statement: public statement
    {
    public:
      select_statement (connection& conn,
                        const std::string& statement,
                        binding& cond,
                        binding& data);

      // Common select interface expected by the generated code.
      //
    public:
      enum result
      {
        success,
        no_data,
        truncated
      };

      void
      execute ();

      // Load next row columns into bound buffers.
      //
      result
      fetch ()
      {
        return next () ? load () : no_data;
      }

      // Reload truncated columns into bound buffers.
      //
      void
      refetch ()
      {
        reload ();
      }

      // Free the result set.
      //
      void
      free_result ()
      {
      }

      // More fine-grained SQLite-specific interface that splits fetch()
      // into next() and load().
      //
    public:
      // Return false if there is no more rows.
      //
      bool
      next ();

      result
      load ();

      void
      reload ();

    private:
      select_statement (const select_statement&);
      select_statement& operator= (const select_statement&);

    private:
      bool done_;
      binding& cond_;
      binding& data_;
    };

    class LIBODB_SQLITE_EXPORT insert_statement: public statement
    {
    public:
      insert_statement (connection& conn,
                        const std::string& statement,
                        binding& data);

      // Return true if successful and false if the row is a duplicate.
      // All other errors are reported by throwing exceptions.
      //
      bool
      execute ();

      unsigned long long
      id ();

    private:
      insert_statement (const insert_statement&);
      insert_statement& operator= (const insert_statement&);

    private:
      binding& data_;
    };

    class LIBODB_SQLITE_EXPORT update_statement: public statement
    {
    public:
      update_statement (connection& conn,
                        const std::string& statement,
                        binding& cond,
                        binding& data);
      void
      execute ();

    private:
      update_statement (const update_statement&);
      update_statement& operator= (const update_statement&);

    private:
      binding& cond_;
      binding& data_;
    };

    class LIBODB_SQLITE_EXPORT delete_statement: public statement
    {
    public:
      delete_statement (connection& conn,
                        const std::string& statement,
                        binding& cond);

      unsigned long long
      execute ();

    private:
      delete_statement (const delete_statement&);
      delete_statement& operator= (const delete_statement&);

    private:
      binding& cond_;
    };
  }
}

#include <odb/post.hxx>

#endif // ODB_SQLITE_STATEMENT_HXX
