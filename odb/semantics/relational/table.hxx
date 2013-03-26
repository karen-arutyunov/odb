// file      : odb/semantics/relational/table.hxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_TABLE_HXX
#define ODB_SEMANTICS_RELATIONAL_TABLE_HXX

#include <odb/semantics/relational/elements.hxx>

namespace semantics
{
  namespace relational
  {
    class table: public qnameable, public uscope
    {
    public:
      table (string const& id): qnameable (id) {}
      table (table const&, qscope&, graph&);
      table (xml::parser&, qscope&, graph&);

      virtual table&
      clone (qscope&, graph&) const;

      virtual string
      kind () const {return "table";}

      virtual void
      serialize (xml::serializer&) const;

      // Resolve ambiguity.
      //
      using qnameable::scope;
    };

    class add_table: public table
    {
    public:
      add_table (string const& id): table (id) {}
      add_table (table const& t, qscope& s, graph& g): table (t, s, g) {}
      add_table (xml::parser& p, qscope& s, graph& g): table (p, s, g) {}

      virtual add_table&
      clone (qscope&, graph&) const;

      virtual string
      kind () const {return "add table";}

      virtual void
      serialize (xml::serializer&) const;
    };

    class drop_table: public qnameable
    {
    public:
      drop_table (string const& id): qnameable (id) {}
      drop_table (drop_table const& t, qscope&, graph& g): qnameable (t, g) {}
      drop_table (xml::parser&, qscope&, graph&);

      virtual drop_table&
      clone (qscope&, graph&) const;

      virtual string
      kind () const {return "drop table";}

      virtual void
      serialize (xml::serializer&) const;
    };

    class alter_table: public qnameable, public uscope
    {
    public:
      alter_table (string const& id): qnameable (id) {}
      alter_table (alter_table const&, qscope&, graph&);
      alter_table (xml::parser&, qscope&, graph&);

      virtual alter_table&
      clone (qscope&, graph&) const;

      virtual string
      kind () const {return "alter table";}

      virtual void
      serialize (xml::serializer&) const;

      // Resolve ambiguity.
      //
      using qnameable::scope;
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_TABLE_HXX
