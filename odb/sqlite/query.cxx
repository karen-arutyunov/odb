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
    }

    query_params& query_params::
    operator= (const query_params& x)
    {
      if (this != &x)
      {
        params_ = x.params_;
        bind_ = x.bind_;
      }

      return *this;
    }

    query_params::binding_type& query_params::
    binding ()
    {
      size_t n (params_.size ());
      binding_type& r (binding_);

      if (n == 0)
        return r; // r.bind and r.count should be 0.

      sqlite::bind* b (&bind_[0]);

      bool inc_ver (false);

      if (r.bind != b || r.count != n)
      {
        r.bind = b;
        r.count = n;
        inc_ver = true;
      }

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

      query_params& p (*parameters_);
      query_params& qp (*q.parameters_);

      p.params_.insert (
        p.params_.end (), qp.params_.begin (), qp.params_.end ());

      p.bind_.insert (p.bind_.end (), qp.bind_.begin (), qp.bind_.end ());

      return *this;
    }

    void query::
    add (details::shared_ptr<query_param> p)
    {
      size_t n (clause_.size ());

      if (n != 0 && clause_[n - 1] != ' ')
        clause_ += ' ';

      clause_ += '?';

      parameters_->params_.push_back (p);
      parameters_->bind_.push_back (sqlite::bind ());
      sqlite::bind* b (&parameters_->bind_.back ());
      memset (b, 0, sizeof (sqlite::bind));

      p->bind (b);
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
