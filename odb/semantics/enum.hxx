// file      : odb/semantics/enum.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_ENUM_HXX
#define ODB_SEMANTICS_ENUM_HXX

#include <vector>
#include <semantics/elements.hxx>

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
    enumerates ()
    {
    }

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
  class enumerator: public instance
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

  public:
    enumerator (path const& file, size_t line, size_t column)
        : node (file, line, column)
    {
    }

    void
    add_edge_right (enumerates& e)
    {
      enumerated_ = &e;
    }

    using instance::add_edge_right;

  private:
    enumerates* enumerated_;
  };

  //
  //
  class enum_: public type
  {
  private:
    typedef std::vector<enumerates*> enumerates_list;

  public:
    typedef enumerates_list::const_iterator enumerates_iterator;

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

  public:
    enum_ (path const& file, size_t line, size_t column)
        : node (file, line, column)
    {
    }

    void
    add_edge_left (enumerates& e)
    {
      enumerates_.push_back (&e);
    }

  private:
    enumerates_list enumerates_;
  };
}

#endif // ODB_SEMANTICS_ENUM_HXX
