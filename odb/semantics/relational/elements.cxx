// file      : odb/semantics/relational/elements.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/relational.hxx>

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
