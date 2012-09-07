// file      : odb/sqlite/database.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h> // WideCharToMultiByte
#endif

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
    using odb::details::transfer_ptr;

    database::
    ~database ()
    {
    }

    database::
    database (const string& name,
              int flags,
              bool foreign_keys,
              const string& vfs,
              transfer_ptr<connection_factory> factory)
        : name_ (name),
          flags_ (flags),
          foreign_keys_ (foreign_keys),
          vfs_ (vfs),
          factory_ (factory.transfer ())
    {
      if (!factory_)
        factory_.reset (new connection_pool_factory ());

      factory_->database (*this);
    }

#ifdef _WIN32
    database::
    database (const wstring& name,
              int flags,
              bool foreign_keys,
              const string& vfs,
              transfer_ptr<connection_factory> factory)
        : flags_ (flags),
          foreign_keys_ (foreign_keys),
          vfs_ (vfs),
          factory_ (factory.transfer ())
    {
      // Convert UTF-16 name to UTF-8 using the WideCharToMultiByte() Win32
      // function.
      //
      int n (
        WideCharToMultiByte (
          CP_UTF8,
          0,
          name.c_str (),
          static_cast<int> (name.size ()),
          0,
          0,
          0,
          0));

      if (n == 0)
        throw database_exception (
          SQLITE_CANTOPEN, SQLITE_CANTOPEN, "unable to open database file");

      // This string is not shared so we are going to modify the underlying
      // buffer directly.
      //
      name_.resize (static_cast<string::size_type> (n));

      n = WideCharToMultiByte (
        CP_UTF8,
        0,
        name.c_str (),
        static_cast<int> (name.size ()),
        const_cast<char*> (name_.c_str ()),
        n,
        0,
        0);

      if (n == 0)
        throw database_exception (
          SQLITE_CANTOPEN, SQLITE_CANTOPEN, "unable to open database file");

      if (!factory_)
        factory_.reset (new connection_pool_factory ());

      factory_->database (*this);
    }
#endif

    database::
    database (int& argc,
              char* argv[],
              bool erase,
              int flags,
              bool foreign_keys,
              const string& vfs,
              transfer_ptr<connection_factory> factory)
        : flags_ (flags),
          foreign_keys_ (foreign_keys),
          vfs_ (vfs),
          factory_ (factory.transfer ())
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

      if (!factory_)
        factory_.reset (new connection_pool_factory ());

      factory_->database (*this);
    }

    void database::
    print_usage (std::ostream& os)
    {
      details::options::print_usage (os);
    }

    transaction_impl* database::
    begin ()
    {
      return new transaction_impl (*this, transaction_impl::deferred);
    }

    transaction_impl* database::
    begin_immediate ()
    {
      return new transaction_impl (*this, transaction_impl::immediate);
    }

    transaction_impl* database::
    begin_exclusive ()
    {
      return new transaction_impl (*this, transaction_impl::exclusive);
    }

    odb::connection* database::
    connection_ ()
    {
      connection_ptr c (factory_->connect ());
      return c.release ();
    }
  }
}
