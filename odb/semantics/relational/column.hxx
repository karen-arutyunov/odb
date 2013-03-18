// file      : odb/semantics/relational/column.hxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_COLUMN_HXX
#define ODB_SEMANTICS_RELATIONAL_COLUMN_HXX

#include <odb/semantics/relational/elements.hxx>
#include <odb/semantics/relational/table.hxx>

namespace semantics
{
  namespace relational
  {
    class contains;

    class column: public unameable
    {
      typedef std::vector<contains*> contained_list;

    public:
      string const&
      type () const {return type_;}

      bool
      null () const {return null_;}

      string const&
      default_ () const {return default__;}

      void
      default_ (string const& d) {default__ = d;}

      string const&
      options () const {return options_;}

      void
      options (string const& o) {options_ = o;}

    public:
      typedef relational::table table_type;

      table_type&
      table () const
      {
        return dynamic_cast<table_type&> (scope ());
      }

      // Key containment.
      //
    public:
      typedef
      pointer_iterator<contained_list::const_iterator>
      contained_iterator;

      contained_iterator
      contained_begin () const {return contained_.begin ();}

      contained_iterator
      contained_end () const {return contained_.end ();}

    public:
      column (string const& id, string const& type, bool null)
          : unameable (id), type_ (type), null_ (null)
      {
      }

      column (column const&, uscope&, graph&);
      column (xml::parser&, uscope&, graph&);

      virtual column&
      clone (uscope&, graph&) const;

      void
      add_edge_right (contains& e)
      {
        contained_.push_back (&e);
      }

      using unameable::add_edge_right;

      virtual string
      kind () const
      {
        return "column";
      }

      virtual void
      serialize (xml::serializer&) const;

    private:
      string type_;
      bool null_;
      string default__;
      string options_;

      contained_list contained_;
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_COLUMN_HXX
