// file      : odb/relational/context.ixx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

namespace relational
{
  inline bool context::
  grow (semantics::class_& c)
  {
    return current ().grow_impl (c);
  }

  inline bool context::
  grow (semantics::data_member& m)
  {
    return current ().grow_impl (m);
  }

  inline bool context::
  grow (semantics::data_member& m, semantics::type& t, string const& kp)
  {
    return current ().grow_impl (m, t, kp);
  }

  inline context::string context::
  quote_id (string const& id) const
  {
    return current ().quote_id_impl (id);
  }
}
