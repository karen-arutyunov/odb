// file      : odb/relational/context.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/context.hxx>

using namespace std;

namespace relational
{
  context::
  context ()
      : data_ (current ().data_)
  {
  }

  context::
  context (data* d)
      : data_ (d)
  {
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
  quote_id_impl (string const& id) const
  {
    string r;
    r.reserve (id.size ());
    r += '"';
    r += id;
    r += '"';
    return r;
  }
}
