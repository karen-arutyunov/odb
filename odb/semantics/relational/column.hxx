// file      : odb/semantics/relational/column.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
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

    class column: public nameable
    {
      typedef std::vector<contains*> contained_list;

    public:
      column (string const& id, string const& type, bool null)
          : nameable (id), type_ (type), null_ (null)
      {
      }

      string const&
      type () const
      {
        return type_;
      }

      bool
      null () const
      {
        return null_;
      }

      string const&
      default_ () const
      {
        return default__;
      }

      void
      default_ (string const& d)
      {
        default__ = d;
      }

      string const&
      options () const
      {
        return options_;
      }

      void
      options (string const& o)
      {
        options_ = o;
      }

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
      contained_begin () const
      {
        return contained_.begin ();
      }

      contained_iterator
      contained_end () const
      {
        return contained_.end ();
      }

    public:
      void
      add_edge_right (contains& e)
      {
        contained_.push_back (&e);
      }

      using nameable::add_edge_right;

      virtual string
      kind () const
      {
        return "column";
      }

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
