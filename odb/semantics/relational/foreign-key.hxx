// file      : odb/semantics/relational/foreign-key.hxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_FOREIGN_KEY_HXX
#define ODB_SEMANTICS_RELATIONAL_FOREIGN_KEY_HXX

#include <odb/semantics/relational/elements.hxx>
#include <odb/semantics/relational/key.hxx>

namespace semantics
{
  namespace relational
  {
    class foreign_key: public unameable, public key
    {
    public:
      enum action
      {
        no_action,
        cascade
      };

      foreign_key (string const& id,
                   qname const& referenced_table,
                   bool deferred,
                   action on_delete = no_action)
          : unameable (id),
            referenced_table_ (referenced_table),
            deferred_ (deferred),
            on_delete_ (on_delete)
      {
      }

    public:
      qname const&
      referenced_table () const
      {
        return referenced_table_;
      }

      typedef std::vector<string> columns;

      columns const&
      referenced_columns () const
      {
        return referenced_columns_;
      }

      columns&
      referenced_columns ()
      {
        return referenced_columns_;
      }

    public:
      bool
      deferred () const
      {
        return deferred_;
      }

    public:
      action
      on_delete () const
      {
        return on_delete_;
      }

    public:
      virtual string
      kind () const
      {
        return "foreign key";
      }

    private:
      qname referenced_table_;
      columns referenced_columns_;
      bool deferred_;
      action on_delete_;
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_FOREIGN_KEY_HXX
