// file      : odb/sqlite/exceptions.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/sqlite/exceptions.hxx>

using namespace std;

namespace odb
{
  namespace sqlite
  {
    //
    // database_exception
    //

    database_exception::
    ~database_exception () throw ()
    {
    }

    database_exception::
    database_exception (int e, int ee, const string& m)
        : error_ (e), extended_error_ (ee), message_ (m)
    {
    }

    const char* database_exception::
    what () const throw ()
    {
      return message_.c_str ();
    }

    //
    // cli_exception
    //

    cli_exception::
    cli_exception (const std::string& what)
        : what_ (what)
    {
    }

    cli_exception::
    ~cli_exception () throw ()
    {
    }

    const char* cli_exception::
    what () const throw ()
    {
      return what_.c_str ();
    }
  }
}
