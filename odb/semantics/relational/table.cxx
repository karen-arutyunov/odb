// file      : odb/semantics/relational/table.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>

#include <odb/semantics/relational/table.hxx>

namespace semantics
{
  namespace relational
  {
    // table
    //
    table::
    table (table const& t, qscope&, graph& g)
        : qnameable (t, g), uscope (t, g)
    {
    }

    table::
    table (xml::parser& p, qscope&, graph& g)
        : qnameable (p, g), uscope (p, g)
    {
    }

    table& table::
    clone (qscope& s, graph& g) const
    {
      return g.new_node<table> (*this, s, g);
    }

    void table::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "table");
      qnameable::serialize_attributes (s);
      uscope::serialize_content (s);
      s.end_element ();
    }

    // add_table
    //
    add_table& add_table::
    clone (qscope& s, graph& g) const
    {
      return g.new_node<add_table> (*this, s, g);
    }

    void add_table::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "add-table");
      table::serialize_attributes (s);
      table::serialize_content (s);
      s.end_element ();
    }

    // drop_table
    //
    drop_table::
    drop_table (xml::parser& p, qscope&, graph& g)
        : qnameable (p, g)
    {
      p.content (xml::parser::empty);
    }

    drop_table& drop_table::
    clone (qscope& s, graph& g) const
    {
      return g.new_node<drop_table> (*this, s, g);
    }

    void drop_table::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "drop-table");
      qnameable::serialize_attributes (s);
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
          qnameable::parser_map_["add-table"] =
            &qnameable::parser_impl<add_table>;
          qnameable::parser_map_["drop-table"] =
            &qnameable::parser_impl<drop_table>;

          using compiler::type_info;

          // table
          //
          {
            type_info ti (typeid (table));
            ti.add_base (typeid (qnameable));
            ti.add_base (typeid (uscope));
            insert (ti);
          }

          // add_table
          //
          {
            type_info ti (typeid (add_table));
            ti.add_base (typeid (table));
            insert (ti);
          }

          // drop_table
          //
          {
            type_info ti (typeid (drop_table));
            ti.add_base (typeid (qnameable));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
