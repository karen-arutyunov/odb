// file      : odb/sqlite/database.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_DATABASE_HXX
#define ODB_SQLITE_DATABASE_HXX

#include <odb/pre.hxx>

#include <sqlite3.h>

#include <string>
#include <memory> // std::auto_ptr
#include <iosfwd> // std::ostream

#include <odb/database.hxx>
#include <odb/details/shared-ptr.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/forward.hxx>
#include <odb/sqlite/connection.hxx>
#include <odb/sqlite/connection-factory.hxx>
#include <odb/sqlite/transaction-impl.hxx>
#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    class LIBODB_SQLITE_EXPORT database: public odb::database
    {
    public:
      typedef sqlite::connection connection_type;

    public:
      database (const std::string& name,
                int flags = SQLITE_OPEN_READWRITE,
                std::auto_ptr<connection_factory> =
                  std::auto_ptr<connection_factory> (0));

      // Extract the database parameters from the command line. The
      // following options are recognized:
      //
      // --database
      // --create
      // --read-only
      // --options-file
      //
      // For more information, see the output of the print_usage() function
      // below. If erase is true, the above options are removed from the argv
      // array and the argc count is updated accordingly. The command line
      // options override the flags passed as an argument. This constructor
      // may throw the cli_exception exception.
      //
      database (int& argc,
                char* argv[],
                bool erase = false,
                int flags = SQLITE_OPEN_READWRITE,
                std::auto_ptr<connection_factory> =
                  std::auto_ptr<connection_factory> (0));

      static void
      print_usage (std::ostream&);


    public:
      const std::string&
      name () const
      {
        return name_;
      }

      int
      flags () const
      {
        return flags_;
      }

    public:
      using odb::database::execute;

      virtual unsigned long long
      execute (const char* statement, std::size_t length);

    public:
      virtual transaction_impl*
      begin ();

      transaction_impl*
      begin_immediate ();

      transaction_impl*
      begin_exclusive ();

    public:
      details::shared_ptr<connection_type>
      connection ();

    public:
      virtual
      ~database ();

    private:
      std::string name_;
      int flags_;
      std::auto_ptr<connection_factory> factory_;
    };
  }
}

#include <odb/sqlite/database.ixx>

#include <odb/post.hxx>

#endif // ODB_SQLITE_DATABASE_HXX
