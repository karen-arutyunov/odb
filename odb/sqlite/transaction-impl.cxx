// file      : odb/sqlite/transaction-impl.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
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
        : odb::transaction_impl (db), lock_ (l)
    {
    }

    transaction_impl::
    transaction_impl (connection_ptr c, lock l)
        : odb::transaction_impl (c->database (), *c),
          connection_ (c),
          lock_ (l)
    {
    }

    transaction_impl::
    ~transaction_impl ()
    {
    }

    void transaction_impl::
    start ()
    {
      // Grab a connection if we don't already have one.
      //
      if (connection_ == 0)
      {
        connection_ = static_cast<database_type&> (database_).connection ();
        odb::transaction_impl::connection_ = connection_.get ();
      }

      statement_cache& sc (connection_->statement_cache ());

      switch (lock_)
      {
      case deferred:
        {
          sc.begin_statement ().execute ();
          break;
        }
      case immediate:
        {
          sc.begin_immediate_statement ().execute ();
          break;
        }
      case exclusive:
        {
          sc.begin_exclusive_statement ().execute ();
          break;
        }
      }
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
