// file      : odb/semantics/relational/table.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_TABLE_HXX
#define ODB_SEMANTICS_RELATIONAL_TABLE_HXX

#include <odb/semantics/relational/elements.hxx>

namespace semantics
{
  namespace relational
  {
    class table: public nameable, public scope
    {
    protected:
      table (string const& id)
          : nameable (id)
      {
      }
    };

    class object_table: public table
    {
    public:
      object_table (string const& id)
          : table (id)
      {
      }

      virtual string
      kind () const
      {
        return "object table";
      }
    };

    class container_table: public table
    {
    public:
      container_table (string const& id)
          : table (id)
      {
      }

      virtual string
      kind () const
      {
        return "container table";
      }
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_TABLE_HXX
