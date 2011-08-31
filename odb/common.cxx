// file      : odb/common.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>

using namespace std;

//
// object_members_base
//

void object_members_base::
traverse_simple (semantics::data_member&)
{
}

void object_members_base::
traverse_composite (semantics::data_member*, semantics::class_& c)
{
  inherits (c);
  names (c);
}

void object_members_base::
traverse_composite_wrapper (semantics::data_member* m,
                            semantics::class_& c,
                            semantics::type*)
{
  traverse_composite (m, c);
}

void object_members_base::
traverse_container (semantics::data_member&, semantics::type&)
{
}

void object_members_base::
traverse_object (semantics::class_& c)
{
  inherits (c);
  names (c);
}

void object_members_base::
traverse (semantics::data_member& m, semantics::class_& c)
{
  traverse_composite_wrapper (&m, c, 0);
}

void object_members_base::
traverse (semantics::class_& c)
{
  bool obj (object (c));

  // Ignore transient bases.
  //
  if (!(obj || context::composite (c)))
    return;

  semantics::class_* prev;
  if (obj)
  {
    prev = context::cur_object;
    context::cur_object = &c;

    if (context::top_object == 0)
      context::top_object = &c;
  }

  if (obj && build_table_prefix_)
  {
    // Don't reset the table prefix if we are traversing a base.
    //
    bool tb (false);

    if (table_prefix_.level == 0)
    {
      table_prefix_.prefix = table_name (c);
      table_prefix_.prefix += '_';
      table_prefix_.level = 1;
      tb = true;
    }

    traverse_object (c);

    if (tb)
    {
      table_prefix_.level = 0;
      table_prefix_.prefix.clear ();
    }
  }
  else
  {
    if (obj)
      traverse_object (c);
    else
      traverse_composite_wrapper (0, c, 0);
  }

  if (obj)
  {
    if (prev == 0)
      context::top_object = 0;

    context::cur_object = prev;
  }
}

void object_members_base::member::
traverse (semantics::data_member& m)
{
  if (transient (m))
    return;

  semantics::type& t (m.type ());

  if (semantics::class_* comp = context::composite_wrapper (t))
  {
    string old_prefix, old_table_prefix;

    if (om_.build_prefix_)
    {
      old_prefix = om_.prefix_;
      om_.prefix_ += om_.public_name (m);
      om_.prefix_ += '_';
    }

    if (om_.build_table_prefix_)
    {
      old_table_prefix = om_.table_prefix_.prefix;

      // If the user provided a table prefix, then use it verbatim. Also
      // drop the top-level table prefix in this case.
      //
      if (m.count ("table"))
      {
        if (om_.table_prefix_.level <= 1)
          om_.table_prefix_.prefix = om_.options.table_prefix ();

        om_.table_prefix_.prefix += m.get<string> ("table");
      }
      // Otherwise use the member name and add an underscore unless it is
      // already there.
      //
      else
      {
        string name (om_.public_name_db (m));
        size_t n (name.size ());

        if (om_.table_prefix_.prefix.empty ())
          om_.table_prefix_.prefix = om_.options.table_prefix ();

        om_.table_prefix_.prefix += name;

        if (n != 0 && name[n - 1] != '_')
          om_.table_prefix_.prefix += '_';
      }

      om_.table_prefix_.level++;
    }

    om_.traverse_composite_wrapper (&m, *comp, (wrapper (t) ? &t : 0));

    if (om_.build_table_prefix_)
    {
      om_.table_prefix_.level--;
      om_.table_prefix_.prefix = old_table_prefix;
    }

    if (om_.build_prefix_)
      om_.prefix_ = old_prefix;
  }
  else if (semantics::type* c = context::container_wrapper (t))
  {
    om_.traverse_container (m, *c);
  }
  else
  {
    om_.traverse_simple (m);
  }
}

//
// object_columns_base
//

void object_columns_base::
flush ()
{
}

void object_columns_base::
traverse_composite (semantics::data_member*, semantics::class_& c)
{
  inherits (c);
  names (c);
}

void object_columns_base::
traverse_object (semantics::class_& c)
{
  inherits (c);
  names (c);
}

void object_columns_base::
traverse (semantics::data_member& m,
          semantics::class_& c,
          string const& key_prefix,
          string const& default_name)
{
  bool custom (m.count (key_prefix + "-column"));
  string name (member_.column_name (m, key_prefix, default_name));

  // If the user provided the column prefix, then use it verbatime.
  // Otherwise, append the underscore, unless it is already there.
  //
  member_.prefix_ = name;

  if (!custom)
  {
    size_t n (name.size ());

    if (n != 0 && name[n - 1] != '_')
      member_.prefix_ += '_';
  }

  traverse_composite (&m, c);

  if (!member_.first_)
    flush ();
}

void object_columns_base::
traverse (semantics::class_& c)
{
  bool obj (object (c));

  // Ignore transient bases.
  //
  if (!(obj || context::composite (c)))
    return;

  bool f (top_level_);

  if (top_level_)
    top_level_ = false;

  semantics::class_* prev;
  if (obj)
  {
    prev = context::cur_object;
    context::cur_object = &c;

    if (context::top_object == 0)
      context::top_object = &c;
  }

  if (obj)
    traverse_object (c);
  else
    traverse_composite (0, c);

  if (obj)
  {
    if (prev == 0)
      context::top_object = 0;

    context::cur_object = prev;
  }

  if (f && !member_.first_)
    flush ();
}

void object_columns_base::member::
traverse (semantics::data_member& m)
{
  if (transient (m))
    return;

  semantics::type& t (m.type ());

  if (semantics::class_* comp = composite_wrapper (t))
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

    oc_.traverse_composite (&m, *comp);

    prefix_ = old_prefix;
  }
  else if (container_wrapper (t))
  {
    // Container gets its own table, so nothing to do here.
    //
  }
  else
  {
    if (oc_.traverse_column (m, prefix_ + column_name (m), first_))
    {
      if (first_)
        first_ = false;
    }
  }
}
