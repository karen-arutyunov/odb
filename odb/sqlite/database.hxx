// file      : odb/sqlite/database.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_DATABASE_HXX
#define ODB_SQLITE_DATABASE_HXX

#include <odb/pre.hxx>

#include <sqlite3.h>

#include <string>
#include <memory> // std::auto_ptr, std::unique_ptr
#include <iosfwd> // std::ostream

#include <odb/database.hxx>
#include <odb/details/unique-ptr.hxx>
#include <odb/details/transfer-ptr.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/forward.hxx>
#include <odb/sqlite/tracer.hxx>
#include <odb/sqlite/connection.hxx>
#include <odb/sqlite/connection-factory.hxx>
#include <odb/sqlite/transaction-impl.hxx>

#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    class transaction_impl;

    class LIBODB_SQLITE_EXPORT database: public odb::database
    {
    public:
      database (const std::string& name,
                int flags = SQLITE_OPEN_READWRITE,
                bool foreign_keys = true,
                const std::string& vfs = "",
                details::transfer_ptr<connection_factory> =
                  details::transfer_ptr<connection_factory> ());

#ifdef _WIN32
      database (const std::wstring& name,
                int flags = SQLITE_OPEN_READWRITE,
                bool foreign_keys = true,
                const std::string& vfs = "",
                details::transfer_ptr<connection_factory> =
                  details::transfer_ptr<connection_factory> ());
#endif

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
                bool foreign_keys = true,
                const std::string& vfs = "",
                details::transfer_ptr<connection_factory> =
                  details::transfer_ptr<connection_factory> ());

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

      bool
      foreign_keys () const
      {
        return foreign_keys_;
      }

      const std::string&
      vfs () const
      {
        return vfs_;
      }

      // Transactions.
      //
    public:
      virtual transaction_impl*
      begin ();

      transaction_impl*
      begin_immediate ();

      transaction_impl*
      begin_exclusive ();

    public:
      connection_ptr
      connection ();

      // SQL statement tracing.
      //
    public:
      typedef sqlite::tracer tracer_type;

      void
      tracer (tracer_type& t)
      {
        odb::database::tracer (t);
      }

      void
      tracer (tracer_type* t)
      {
        odb::database::tracer (t);
      }

      using odb::database::tracer;

    public:
      virtual
      ~database ();

    protected:
      virtual odb::connection*
      connection_ ();

    private:
      std::string name_;
      int flags_;
      bool foreign_keys_;
      std::string vfs_;
      details::unique_ptr<connection_factory> factory_;
    };
  }
}

#include <odb/sqlite/database.ixx>

#include <odb/post.hxx>

#endif // ODB_SQLITE_DATABASE_HXX
