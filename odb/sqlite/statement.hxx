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
//@@ #include <odb/sqlite/binding.hxx>

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
  }
}

#include <odb/post.hxx>

#endif // ODB_SQLITE_STATEMENT_HXX
