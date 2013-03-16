// file      : odb/semantics/relational/primary-key.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/relational.hxx>

namespace semantics
{
  namespace relational
  {
    primary_key::
    primary_key (xml::parser& p, uscope& s, graph& g)
        : key (p, s, g),
          auto__ (p.attribute ("auto", false))
    {
    }

    void primary_key::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "primary-key");
      key::serialize_attributes (s);
      if (auto_ ())
        s.attribute ("auto", true);
      key::serialize_content (s);
      s.end_element ();
    }

    // type info
    //
    namespace
    {
      struct init
      {
        init ()
        {
          unameable::parser_map_["primary-key"] =
            &unameable::parser_impl<primary_key>;

          using compiler::type_info;

          {
            type_info ti (typeid (primary_key));
            ti.add_base (typeid (unameable));
            ti.add_base (typeid (key));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
