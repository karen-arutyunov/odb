// file      : odb/semantics/relational/table.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/relational.hxx>

namespace semantics
{
  namespace relational
  {
    table::
    table (xml::parser& p, qscope&, graph& g)
        : qnameable (p, g), uscope (p, g)
    {
    }

    void table::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "table");
      qnameable::serialize_attributes (s);
      uscope::serialize_content (s);
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
          qnameable::parser_map_["table"] = &qnameable::parser_impl<table>;

          using compiler::type_info;

          {
            type_info ti (typeid (table));
            ti.add_base (typeid (qnameable));
            ti.add_base (typeid (uscope));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
