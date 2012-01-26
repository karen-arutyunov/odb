// file      : odb/semantics/relational/elements.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/elements.hxx>

namespace semantics
{
  namespace relational
  {
    // duplicate_name
    //
    template <>
    duplicate_name::
    duplicate_name (uscope& s, unameable& o, unameable& d)
        : scope (s), orig (o), dup (d),
          orig_name (o.name ()), dup_name (d.name ())
    {
    }

    template <>
    duplicate_name::
    duplicate_name (qscope& s, qnameable& o, qnameable& d)
        : scope (s), orig (o), dup (d),
          orig_name (o.name ().string ()), dup_name (d.name ().string ())
    {
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
