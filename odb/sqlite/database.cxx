// file      : odb/sqlite/database.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <sstream>

#include <odb/sqlite/database.hxx>
#include <odb/sqlite/connection.hxx>
#include <odb/sqlite/connection-factory.hxx>
#include <odb/sqlite/error.hxx>
#include <odb/sqlite/exceptions.hxx>

#include <odb/sqlite/details/options.hxx>

using namespace std;

namespace odb
{
  namespace sqlite
  {
    database::
    ~database ()
    {
    }

    database::
    database (const string& name,
              int flags,
              auto_ptr<connection_factory> factory)
        : name_ (name), flags_ (flags), factory_ (factory)
    {
      if (factory_.get () == 0)
        factory_.reset (new connection_pool_factory ());

      factory_->database (*this);
    }

    database::
    database (int& argc,
              char* argv[],
              bool erase,
              int flags,
              std::auto_ptr<connection_factory> factory)
        : flags_ (flags), factory_ (factory)
    {
      using namespace details;

      try
      {
        cli::argv_file_scanner scan (argc, argv, "--options-file", erase);
        options ops (scan, cli::unknown_mode::skip, cli::unknown_mode::skip);

        name_ = ops.database ();

        if (ops.create ())
          flags_ |= SQLITE_OPEN_CREATE;

        if (ops.read_only ())
          flags_ = (flags_ & ~SQLITE_OPEN_READWRITE) | SQLITE_OPEN_READONLY;
      }
      catch (const cli::exception& e)
      {
        ostringstream ostr;
        ostr << e;
        throw cli_exception (ostr.str ());
      }

      if (factory_.get () == 0)
        factory_.reset (new connection_pool_factory ());

      factory_->database (*this);
    }

    void database::
    print_usage (std::ostream& os)
    {
      details::options::print_usage (os);
    }

    odb::connection* database::
    connection_ ()
    {
      connection_ptr c (factory_->connect ());
      return c.release ();
    }
  }
}
