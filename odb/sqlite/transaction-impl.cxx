// file      : odb/sqlite/transaction-impl.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/sqlite/database.hxx>
#include <odb/sqlite/connection.hxx>
#include <odb/sqlite/statement.hxx>
#include <odb/sqlite/statement-cache.hxx>
#include <odb/sqlite/transaction-impl.hxx>

namespace odb
{
  namespace sqlite
  {
    transaction_impl::
    transaction_impl (database_type& db)
        : odb::transaction_impl (db), connection_ (db.connection ())
    {
      connection_->statement_cache ().begin_statement ().execute ();
    }

    transaction_impl::
    ~transaction_impl ()
    {
    }

    void transaction_impl::
    commit ()
    {
      connection_->statement_cache ().commit_statement ().execute ();

      // Release the connection.
      //
      //connection_.reset ();
    }

    void transaction_impl::
    rollback ()
    {
      connection_->statement_cache ().rollback_statement ().execute ();

      // Release the connection.
      //
      //connection_.reset ();
    }
  }
}
