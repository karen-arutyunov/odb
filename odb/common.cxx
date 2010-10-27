// file      : odb/common.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>

using namespace std;

//
// object_columns_base
//

void object_columns_base::
composite (semantics::data_member& m)
{
  dispatch (m.type ());
}

void object_columns_base::
traverse (semantics::class_& c)
{
  inherits (c);
  names (c);
}

void object_columns_base::member::
traverse (semantics::data_member& m)
{
  if (m.count ("transient"))
    return;

  if (comp_value (m.type ()))
  {
    string old_prefix (prefix_);

    bool custom (m.count ("column"));
    string name (column_name (m));

    // If the user provided the column prefix, then use it verbatime.
    // Otherwise, append the underscore, unless it is already there.
    //
    prefix_ += name;

    if (!custom)
    {
      size_t n (name.size ());

      if (n != 0 && name[n - 1] != '_')
        prefix_ += '_';
    }

    oc_.composite (m);

    prefix_ = old_prefix;
  }
  else
  {
    oc_.column (m, prefix_ + column_name (m), first_);

    if (first_)
      first_ = false;
  }
}
