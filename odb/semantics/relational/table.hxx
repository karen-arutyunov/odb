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
      table (xml::parser&, qscope&, graph&);

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
      add_table (xml::parser& p, qscope& s, graph& g): table (p, s, g) {}

      virtual string
      kind () const {return "add table";}

      virtual void
      serialize (xml::serializer&) const;
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_TABLE_HXX
