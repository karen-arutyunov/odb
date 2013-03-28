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

      foreign_key (foreign_key const&, uscope&, graph&);
      foreign_key (xml::parser&, uscope&, graph&);

      virtual foreign_key&
      clone (uscope&, graph&) const;

      virtual string
      kind () const
      {
        return "foreign key";
      }

      virtual void
      serialize (xml::serializer&) const;

    protected:
      void
      serialize_attributes (xml::serializer&) const;

      void
      serialize_content (xml::serializer&) const;

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

    class add_foreign_key: public foreign_key
    {
    public:
      add_foreign_key (string const& id,
                       qname const& rt,
                       bool d,
                       action_type od = no_action)
          : foreign_key (id, rt, d, od) {}
      add_foreign_key (foreign_key const& fk, uscope& s, graph& g)
          : foreign_key (fk, s, g) {}
      add_foreign_key (xml::parser& p, uscope& s, graph& g)
          : foreign_key (p, s, g) {}

      virtual add_foreign_key&
      clone (uscope&, graph&) const;

      virtual string
      kind () const {return "add foreign key";}

      virtual void
      serialize (xml::serializer&) const;
    };

    class drop_foreign_key: public unameable
    {
    public:
      drop_foreign_key (string const& id): unameable (id) {}
      drop_foreign_key (drop_foreign_key const& dfk, uscope&, graph& g)
          : unameable (dfk, g) {}
      drop_foreign_key (xml::parser&, uscope&, graph&);

      virtual drop_foreign_key&
      clone (uscope&, graph&) const;

      virtual string
      kind () const {return "drop foreign key";}

      virtual void
      serialize (xml::serializer&) const;
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_FOREIGN_KEY_HXX
