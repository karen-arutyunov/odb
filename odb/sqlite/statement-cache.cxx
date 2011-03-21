// file      : odb/sqlite/statement-cache.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/sqlite/statement-cache.hxx>

namespace odb
{
  using namespace details;

  namespace sqlite
  {
    statement_cache::
    statement_cache (connection& conn)
        : conn_ (conn),
          // String lengths below include '\0', as per SQLite manual
          // suggestions.
          //
          begin_ (new (shared) simple_statement (conn, "BEGIN", 6)),
          commit_ (new (shared) simple_statement (conn, "COMMIT", 7)),
          rollback_ (new (shared) simple_statement (conn, "ROLLBACK", 9))
    {
    }
  }
}
