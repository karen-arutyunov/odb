// file      : odb/semantics/relational/column.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/relational.hxx>

namespace semantics
{
  namespace relational
  {
    column::
    column (xml::parser& p, uscope&, graph& g)
        : unameable (p, g),
          type_ (p.attribute ("type", string ())),
          null_ (p.attribute<bool> ("null")),
          default__ (p.attribute ("default", string ())),
          options_ (p.attribute ("options", string ()))
    {
      p.content (xml::parser::empty);
    }

    void column::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "column");
      unameable::serialize_attributes (s);

      s.attribute ("type", type ());
      s.attribute ("null", null ()); // Output even if false.

      if (!default_ ().empty ())
        s.attribute ("default", default_ ());

      if (!options ().empty ())
        s.attribute ("options", options ());

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
          unameable::parser_map_["column"] = &unameable::parser_impl<column>;

          using compiler::type_info;

          {
            type_info ti (typeid (column));
            ti.add_base (typeid (unameable));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
