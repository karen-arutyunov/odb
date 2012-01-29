// file      : odb/sqlite/statement-cache.cxx
// copyright : Copyright (c) 2005-2012 Code Synthesis Tools CC
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
          begin_ (new (shared) generic_statement (conn_, "BEGIN", 6)),
          commit_ (new (shared) generic_statement (conn_, "COMMIT", 7)),
          rollback_ (new (shared) generic_statement (conn_, "ROLLBACK", 9))
    {
      rollback_->cached (true);
      commit_->cached (true);
      begin_->cached (true);
    }

    void statement_cache::
    begin_immediate_statement_ () const
    {
      begin_immediate_.reset (
        new (shared) generic_statement (conn_, "BEGIN IMMEDIATE", 16));
      begin_immediate_->cached (true);
    }

    void statement_cache::
    begin_exclusive_statement_ () const
    {
      begin_exclusive_.reset (
        new (shared) generic_statement (conn_, "BEGIN EXCLUSIVE", 16));
      begin_exclusive_->cached (true);
    }
  }
}
