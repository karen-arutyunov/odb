// file      : odb/relational/context.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>

#include <odb/relational/context.hxx>

using namespace std;

namespace relational
{
  context* context::current_;

  context::
  ~context ()
  {
    if (current_ == this)
      current_ = 0;
  }

  context::
  context ()
      : data_ (current ().data_),
        model (current ().model),
        generate_grow (current ().generate_grow),
        need_alias_as (current ().need_alias_as),
        insert_send_auto_id (current ().insert_send_auto_id),
        bind_vector (data_->bind_vector_),
        truncated_vector (data_->truncated_vector_)
  {
  }

  context::
  context (data* d, sema_rel::model* m)
      : data_ (d),
        model (m),
        bind_vector (data_->bind_vector_),
        truncated_vector (data_->truncated_vector_)
  {
    assert (current_ == 0);
    current_ = this;
  }

  bool context::
  grow_impl (semantics::class_&)
  {
    return false;
  }

  bool context::
  grow_impl (semantics::data_member&)
  {
    return false;
  }

  bool context::
  grow_impl (semantics::data_member&,
             semantics::type&,
             string const&)
  {
    return false;
  }

  string context::
  quote_string_impl (string const& s) const
  {
    string r;
    r.reserve (s.size ());
    r += '\'';

    for (string::size_type i (0), n (s.size ()); i < n; ++i)
    {
      if (s[i] == '\'')
        r += "''";
      else
        r += s[i];
    }

    r += '\'';
    return r;
  }

  string context::
  quote_id_impl (qname const& id) const
  {
    string r;

    bool f (true);
    for (qname::iterator i (id.begin ()); i < id.end (); ++i)
    {
      if (i->empty ())
        continue;

      if (f)
        f = false;
      else
        r += '.';

      r += '"';
      r += *i;
      r += '"';
    }

    return r;
  }
}
