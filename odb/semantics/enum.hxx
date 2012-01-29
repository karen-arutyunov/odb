// file      : odb/semantics/enum.hxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_ENUM_HXX
#define ODB_SEMANTICS_ENUM_HXX

#include <vector>
#include <odb/semantics/elements.hxx>

namespace semantics
{
  class enum_;
  class enumerator;

  class enumerates: public edge
  {
  public:
    typedef semantics::enum_ enum_type;
    typedef semantics::enumerator enumerator_type;

    enum_type&
    enum_ () const
    {
      return *enum__;
    }

    enumerator_type&
    enumerator () const
    {
      return *enumerator_;
    }

  public:
    enumerates ();

    void
    set_left_node (enum_type& n)
    {
      enum__ = &n;
    }

    void
    set_right_node (enumerator_type& n)
    {
      enumerator_ = &n;
    }

  protected:
    enum_type* enum__;
    enumerator_type* enumerator_;
  };

  //
  //
  class enumerator: public nameable, public instance
  {
  public:
    typedef semantics::enum_ enum_type;

    enum_type&
    enum_ () const
    {
      return enumerated_->enum_ ();
    }

    enumerates&
    enumerated () const
    {
      return *enumerated_;
    }

    // If the enumeration is signed, then this value should be re-
    // interpreted as signed.
    //
    unsigned long long
    value () const
    {
      return value_;
    }

  public:
    enumerator (path const&,
                size_t line,
                size_t column,
                tree,
                unsigned long long value);

    void
    add_edge_right (enumerates& e)
    {
      enumerated_ = &e;
    }

    using nameable::add_edge_right;

  private:
    unsigned long long value_;
    enumerates* enumerated_;
  };

  //
  //
  class enum_: public type
  {
  private:
    typedef std::vector<enumerates*> enumerates_list;

  public:
    typedef
    pointer_iterator<enumerates_list::const_iterator>
    enumerates_iterator;

    enumerates_iterator
    enumerates_begin () const
    {
      return enumerates_.begin ();
    }

    enumerates_iterator
    enumerates_end () const
    {
      return enumerates_.end ();
    }

    bool
    unsigned_ () const
    {
      return unsigned__;
    }

  public:
    enum_ (path const&, size_t line, size_t column, tree, bool unsigned_);

    void
    add_edge_left (enumerates& e)
    {
      enumerates_.push_back (&e);
    }

  private:
    bool unsigned__;
    enumerates_list enumerates_;
  };
}

#endif // ODB_SEMANTICS_ENUM_HXX
