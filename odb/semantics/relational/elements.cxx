// file      : odb/semantics/relational/elements.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/elements.hxx>
#include <odb/semantics/relational/column.hxx>
#include <odb/semantics/relational/primary-key.hxx>

namespace semantics
{
  namespace relational
  {
    // scope
    //

    scope::names_iterator scope::
    find (string const& name)
    {
      names_map::iterator i (names_map_.find (name));

      if (i == names_map_.end ())
        return names_.end ();
      else
        return i->second;
    }

    scope::names_const_iterator scope::
    find (string const& name) const
    {
      names_map::const_iterator i (names_map_.find (name));

      if (i == names_map_.end ())
        return names_.end ();
      else
        return names_const_iterator (i->second);
    }

    scope::names_iterator scope::
    find (names const& e)
    {
      names_iterator_map::iterator i (iterator_map_.find (&e));
      return i != iterator_map_.end () ? i->second : names_.end ();
    }

    scope::names_const_iterator scope::
    find (names const& e) const
    {
      names_iterator_map::const_iterator i (iterator_map_.find (&e));
      return i != iterator_map_.end () ? i->second : names_.end ();
    }

    void scope::
    add_edge_left (names& e)
    {
      nameable& n (e.nameable ());
      string const& name (e.name ());

      names_map::iterator i (names_map_.find (name));

      if (i == names_map_.end ())
      {
        names_list::iterator i;

        // We want the order to be columns first, then the primary key,
        // and then the foreign keys.
        //
        if (n.is_a<column> ())
          i = names_.insert (first_key_, &e);
        else
        {
          if (n.is_a<primary_key> ())
            first_key_ = i = names_.insert (first_key_, &e);
          else
          {
            i = names_.insert (names_.end (), &e);

            if (first_key_ == names_.end ())
              first_key_ = i;
          }
        }

        names_map_[name] = i;
        iterator_map_[&e] = i;
      }
      else
        throw duplicate_name (*this, (*i->second)->nameable (), n);
    }

    // type info
    //
    namespace
    {
      struct init
      {
        init ()
        {
          using compiler::type_info;

          // node
          //
          insert (type_info (typeid (node)));

          // edge
          //
          insert (type_info (typeid (edge)));

          // names
          //
          {
            type_info ti (typeid (names));
            ti.add_base (typeid (edge));
            insert (ti);
          }

          // nameable
          //
          {
            type_info ti (typeid (nameable));
            ti.add_base (typeid (node));
            insert (ti);
          }

          // scope
          //
          {
            type_info ti (typeid (scope));
            ti.add_base (typeid (node));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
