// file      : odb/semantics/relational/foreign-key.hxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_FOREIGN_KEY_HXX
#define ODB_SEMANTICS_RELATIONAL_FOREIGN_KEY_HXX

#include <iosfwd>

#include <odb/semantics/relational/elements.hxx>
#include <odb/semantics/relational/key.hxx>

namespace semantics
{
  namespace relational
  {
    class foreign_key: public key
    {
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

      enum action_type
      {
        no_action,
        cascade
      };

      action_type
      on_delete () const
      {
        return on_delete_;
      }

    public:
      foreign_key (string const& id,
                   qname const& referenced_table,
                   bool deferred,
                   action_type on_delete = no_action)
          : key (id),
            referenced_table_ (referenced_table),
            deferred_ (deferred),
            on_delete_ (on_delete)
      {
      }

      foreign_key (xml::parser&, uscope&, graph&);

      virtual string
      kind () const
      {
        return "foreign key";
      }

      virtual void
      serialize (xml::serializer&) const;

    private:
      qname referenced_table_;
      columns referenced_columns_;
      bool deferred_;
      action_type on_delete_;
    };

    std::ostream&
    operator<< (std::ostream&, foreign_key::action_type);

    std::istream&
    operator>> (std::istream&, foreign_key::action_type&);
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_FOREIGN_KEY_HXX
