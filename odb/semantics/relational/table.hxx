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
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_TABLE_HXX
