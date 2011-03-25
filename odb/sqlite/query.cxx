// file      : odb/sqlite/query.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cstddef> // std::size_t
#include <cstring> // std::memset

#include <odb/sqlite/query.hxx>

using namespace std;

namespace odb
{
  namespace sqlite
  {
    // query_param
    //

    query_param::
    ~query_param ()
    {
    }

    // query_params
    //

    query_params::
    query_params (const query_params& x)
        : details::shared_base (x),
          params_ (x.params_), bind_ (x.bind_), binding_ (0, 0)
    {
      // Here and below we want to maintain up to date binding info so
      // that the call to binding() below is an immutable operation,
      // provided the query does not have any by-reference parameters.
      // This way a by-value-only query can be shared between multiple
      // threads without the need for synchronization.
      //
      if (size_t n = bind_.size ())
      {
        binding_.bind = &bind_[0];
        binding_.count = n;
        binding_.version++;
      }
    }

    query_params& query_params::
    operator= (const query_params& x)
    {
      if (this != &x)
      {
        params_ = x.params_;
        bind_ = x.bind_;

        size_t n (bind_.size ());
        binding_.bind = n != 0 ? &bind_[0] : 0;
        binding_.count = n;
        binding_.version++;
      }

      return *this;
    }

    query_params& query_params::
    operator+= (const query_params& x)
    {
      size_t n (bind_.size ());

      params_.insert (params_.end (), x.params_.begin (), x.params_.end ());
      bind_.insert (bind_.end (), x.bind_.begin (), x.bind_.end ());

      if (n != bind_.size ())
      {
        binding_.bind = &bind_[0];
        binding_.count = bind_.size ();
        binding_.version++;
      }

      return *this;
    }

    void query_params::
    add (details::shared_ptr<query_param> p)
    {
      params_.push_back (p);
      bind_.push_back (sqlite::bind ());
      binding_.bind = &bind_[0];
      binding_.count = bind_.size ();
      binding_.version++;

      sqlite::bind* b (&bind_.back ());
      memset (b, 0, sizeof (sqlite::bind));
      p->bind (b);
    }

    query_params::binding_type& query_params::
    binding ()
    {
      size_t n (params_.size ());
      binding_type& r (binding_);

      if (n == 0)
        return r;

      bool inc_ver (false);
      sqlite::bind* b (&bind_[0]);

      for (size_t i (0); i < n; ++i)
      {
        query_param& p (*params_[i]);

        if (p.reference ())
        {
          if (p.init ())
          {
            p.bind (b + i);
            inc_ver = true;
          }
        }
      }

      if (inc_ver)
        r.version++;

      return r;
    }

    // query
    //

    query::
    query (const query& q)
        : clause_ (q.clause_),
          parameters_ (new (details::shared) query_params (*q.parameters_))
    {
    }

    query& query::
    operator= (const query& q)
    {
      if (this != &q)
      {
        clause_ = q.clause_;
        *parameters_ = *q.parameters_;
      }

      return *this;
    }

    query& query::
    operator+= (const query& q)
    {
      size_t n (clause_.size ());

      if (n != 0 && clause_[n - 1] != ' ' &&
          !q.clause_.empty () && q.clause_[0] != ' ')
        clause_ += ' ';

      clause_ += q.clause_;
      *parameters_ += *q.parameters_;

      return *this;
    }

    void query::
    add (details::shared_ptr<query_param> p)
    {
      size_t n (clause_.size ());

      if (n != 0 && clause_[n - 1] != ' ')
        clause_ += ' ';

      clause_ += '?';
      parameters_->add (p);
    }

    std::string query::
    clause () const
    {
      if (clause_.empty () ||
          clause_.compare (0, 6, "WHERE ") == 0 ||
          clause_.compare (0, 9, "ORDER BY ") == 0 ||
          clause_.compare (0, 9, "GROUP BY ") == 0 ||
          clause_.compare (0, 7, "HAVING ") == 0)
        return clause_;
      else
        return "WHERE " + clause_;
    }
  }
}
