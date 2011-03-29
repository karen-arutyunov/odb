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
    transaction_impl (database_type& db, lock l)
        : odb::transaction_impl (db), connection_ (db.connection ())
    {
      statement_cache& c (connection_->statement_cache ());

      switch (l)
      {
      case deferred:
        {
          c.begin_statement ().execute ();
          break;
        }
      case immediate:
        {
          c.begin_immediate_statement ().execute ();
          break;
        }
      case exclusive:
        {
          c.begin_exclusive_statement ().execute ();
          break;
        }
      }
    }

    transaction_impl::
    ~transaction_impl ()
    {
    }

    void transaction_impl::
    commit ()
    {
      // Reset active and finilize uncached statements. Active statements
      // will prevent COMMIT from completing (write statements) or releasing
      // the locks (read statements). Finilization of uncached statements is
      // needed to release the connection.
      //
      connection_->clear ();

      connection_->statement_cache ().commit_statement ().execute ();

      // Release the connection.
      //
      connection_.reset ();
    }

    void transaction_impl::
    rollback ()
    {
      // Reset active and finilize uncached statements. Active statements
      // will prevent ROLLBACK from completing. Finilization of uncached
      // statements is needed to release the connection.
      //
      connection_->clear ();

      connection_->statement_cache ().rollback_statement ().execute ();

      // Release the connection.
      //
      connection_.reset ();
    }
  }
}
