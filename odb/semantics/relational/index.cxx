// file      : odb/semantics/relational/index.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/relational.hxx>

namespace semantics
{
  namespace relational
  {
    index::
    index (xml::parser& p, uscope& s, graph& g)
        : key (p, s, g),
        type_ (p.attribute ("type", string ())),
        method_ (p.attribute ("method", string ())),
        options_ (p.attribute ("options", string ()))
    {
    }

    void index::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "index");
      key::serialize_attributes (s);

      if (!type ().empty ())
        s.attribute ("type", type ());

      if (!method ().empty ())
        s.attribute ("method", method ());

      if (!options ().empty ())
        s.attribute ("options", options ());

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
          unameable::parser_map_["index"] = &unameable::parser_impl<index>;

          using compiler::type_info;

          {
            type_info ti (typeid (index));
            ti.add_base (typeid (key));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
