// file      : odb/sqlite/error.cxx
// copyright : Copyright (c) 2005-2012 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <sqlite3.h>

#include <new>    // std::bad_alloc
#include <string>

#include <odb/sqlite/connection.hxx>
#include <odb/sqlite/exceptions.hxx>

using namespace std;

namespace odb
{
  namespace sqlite
  {
    void
    translate_error (int e, connection& c)
    {
      sqlite3* h (c.handle ());
      int ee (sqlite3_extended_errcode (h));
      string m;

      switch (e)
      {
      case SQLITE_NOMEM:
        {
          throw bad_alloc ();
        }
      case SQLITE_MISUSE:
        {
          // In case of SQLITE_MISUSE, error code/message may or may not
          // be set.
          //
          ee = e;
          m = "SQLite API misuse";
          break;
        }
      case SQLITE_LOCKED:
        {
          if (ee != SQLITE_LOCKED_SHAREDCACHE)
            throw deadlock (); // The DROP TABLE special case.

          // Getting SQLITE_LOCKED_SHAREDCACHE here means we don't have
          // the unlock notify support. Translate this to timeout.
          //
          throw timeout ();
        }
      case SQLITE_BUSY:
      case SQLITE_IOERR:
        {
          if (e != SQLITE_IOERR || ee == SQLITE_IOERR_BLOCKED)
            throw timeout ();

          // Fall throught.
        }
      default:
        {
          m = sqlite3_errmsg (h);
          break;
        }
      }

      throw database_exception (e, ee, m);
    }
  }
}
