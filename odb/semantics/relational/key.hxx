// file      : odb/semantics/relational/key.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_KEY_HXX
#define ODB_SEMANTICS_RELATIONAL_KEY_HXX

#include <odb/semantics/relational/elements.hxx>

namespace semantics
{
  namespace relational
  {
    class key;
    class column;

    class contains: public edge
    {
    public:
      typedef relational::key key_type;
      typedef relational::column column_type;

      key_type&
      key () const
      {
        return *key_;
      }

      column_type&
      column () const
      {
        return *column_;
      }

    public:
      void
      set_left_node (key_type& n)
      {
        key_ = &n;
      }

      void
      set_right_node (column_type& n)
      {
        column_ = &n;
      }

    protected:
      key_type* key_;
      column_type* column_;
    };

    class key: public unameable
    {
      typedef std::vector<contains*> contains_list;

    public:
      typedef
      pointer_iterator<contains_list::const_iterator>
      contains_iterator;

      contains_iterator
      contains_begin () const
      {
        return contains_.begin ();
      }

      contains_iterator
      contains_end () const
      {
        return contains_.end ();
      }

      contains_list::size_type
      contains_size () const
      {
        return contains_.size ();
      }

    public:
      key (std::string const& id): unameable (id) {}

      void
      add_edge_left (contains& e)
      {
        contains_.push_back (&e);
      }

    private:
      contains_list contains_;
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_KEY_HXX
