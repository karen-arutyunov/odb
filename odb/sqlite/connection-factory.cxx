// file      : odb/sqlite/connection-factory.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/details/lock.hxx>

#include <odb/sqlite/database.hxx>
#include <odb/sqlite/connection-factory.hxx>

using namespace std;

namespace odb
{
  using namespace details;

  namespace sqlite
  {
    //
    // connection_factory
    //

    connection_factory::
    ~connection_factory ()
    {
    }

    //
    // new_connection_factory
    //

    shared_ptr<connection> new_connection_factory::
    connect ()
    {
      return shared_ptr<connection> (
        new (shared) connection (*db_, extra_flags_));
    }

    void new_connection_factory::
    database (database_type& db)
    {
      db_ = &db;

      // Unless explicitly disabled, enable shared cache.
      //
      if ((db_->flags () & SQLITE_OPEN_PRIVATECACHE) == 0)
        extra_flags_ |= SQLITE_OPEN_SHAREDCACHE;
    }

    //
    // connection_pool_factory
    //

    connection_pool_factory::
    ~connection_pool_factory ()
    {
      // Wait for all the connections currently in use to return to the pool.
      //
      lock l (mutex_);
      while (in_use_ != 0)
      {
        waiters_++;
        cond_.wait ();
        waiters_--;
      }
    }

    shared_ptr<connection> connection_pool_factory::
    connect ()
    {
      lock l (mutex_);

      while (true)
      {
        // See if we have a spare connection.
        //
        if (connections_.size () != 0)
        {
          shared_ptr<pooled_connection> c (connections_.back ());
          connections_.pop_back ();

          c->pool_ = this;
          in_use_++;
          return c;
        }

        // See if we can create a new one.
        //
        if(max_ == 0 || in_use_ < max_)
        {
          shared_ptr<pooled_connection> c (
            new (shared) pooled_connection (*db_, extra_flags_, this));
          in_use_++;
          return c;
        }

        // Wait until someone releases a connection.
        //
        waiters_++;
        cond_.wait ();
        waiters_--;
      }
    }

    void connection_pool_factory::
    database (database_type& db)
    {
      db_ = &db;

      // Unless explicitly disabled, enable shared cache.
      //
      if ((db_->flags () & SQLITE_OPEN_PRIVATECACHE) == 0)
        extra_flags_ |= SQLITE_OPEN_SHAREDCACHE;

      if (min_ > 0)
      {
        connections_.reserve (min_);

        for(size_t i (0); i < min_; ++i)
        {
          connections_.push_back (
            shared_ptr<pooled_connection> (
              new (shared) pooled_connection (*db_, extra_flags_, 0)));
        }
      }
    }

    bool connection_pool_factory::
    release (pooled_connection* c)
    {
      c->pool_ = 0;

      lock l (mutex_);

      // Determine if we need to keep or free this connection.
      //
      bool keep (waiters_ != 0 ||
                 min_ == 0 ||
                 (connections_.size () + in_use_ <= min_));

      in_use_--;

      if (keep)
        connections_.push_back (
          shared_ptr<pooled_connection> (inc_ref (c)));

      if (waiters_ != 0)
        cond_.signal ();

      return !keep;
    }

    //
    // connection_pool_factory::pooled_connection
    //

    connection_pool_factory::pooled_connection::
    pooled_connection (database_type& db,
                       int extra_flags,
                       connection_pool_factory* pool)
        : connection (db, extra_flags), pool_ (pool)
    {
      callback_.arg = this;
      callback_.zero_counter = &zero_counter;
      shared_base::callback_ = &callback_;
    }

    bool connection_pool_factory::pooled_connection::
    zero_counter (void* arg)
    {
      pooled_connection* c (static_cast<pooled_connection*> (arg));
      return c->pool_ ? c->pool_->release (c) : true;
    }
  }
}
