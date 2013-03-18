// file      : odb/semantics/relational/elements.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/elements.hxx>
#include <odb/semantics/relational/column.hxx>
#include <odb/semantics/relational/primary-key.hxx>

namespace semantics
{
  namespace relational
  {
    string const xmlns = "http://www.codesynthesis.com/xmlns/odb/changelog";

    // duplicate_name
    //
    template <>
    duplicate_name::
    duplicate_name (uscope& s, unameable& o, unameable& d)
        : scope (s), orig (o), dup (d), name (o.name ())
    {
    }

    template <>
    duplicate_name::
    duplicate_name (qscope& s, qnameable& o, qnameable& d)
        : scope (s), orig (o), dup (d), name (o.name ().string ())
    {
    }

    // scope<uname>
    //
    template <>
    void scope<uname>::
    add_edge_left (names_type& e)
    {
      nameable_type& n (e.nameable ());
      name_type const& name (e.name ());

      typename names_map::iterator i (names_map_.find (name));

      if (i == names_map_.end ())
      {
        typename names_list::iterator i;

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
            type_info ti (typeid (unames));
            ti.add_base (typeid (edge));
            insert (ti);
          }

          {
            type_info ti (typeid (qnames));
            ti.add_base (typeid (edge));
            insert (ti);
          }

          // nameable
          //
          {
            type_info ti (typeid (unameable));
            ti.add_base (typeid (node));
            insert (ti);
          }

          {
            type_info ti (typeid (qnameable));
            ti.add_base (typeid (node));
            insert (ti);
          }

          // scope
          //
          {
            type_info ti (typeid (uscope));
            ti.add_base (typeid (node));
            insert (ti);
          }

          {
            type_info ti (typeid (qscope));
            ti.add_base (typeid (node));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
