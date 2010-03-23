// file      : odb/semantics/class.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_CLASS_HXX
#define ODB_SEMANTICS_CLASS_HXX

#include <vector>
#include <semantics/elements.hxx>

namespace semantics
{
  class class_;

  //
  //
  class data_member: public nameable, public instance
  {
  public:
    data_member (path const& file, size_t line, size_t column)
        : node (file, line, column)
    {
    }
  };

  //
  //
  class inherits: public edge
  {
  public:
    typedef semantics::access access_type;

    class_&
    base () const
    {
      return *base_;
    }

    class_&
    derived () const
    {
      return *derived_;
    }

    bool
    virtual_ () const
    {
      return virt_;
    }

    access_type
    access () const
    {
      return access_;
    }

  public:
    inherits (access_type access, bool virt)
        : virt_ (virt), access_ (access)
    {
    }

    void
    set_left_node (class_& n)
    {
      derived_ = &n;
    }

    void
    set_right_node (class_& n)
    {
      base_ = &n;
    }

  protected:
    bool virt_;
    access_type access_;

    class_* base_;
    class_* derived_;
  };

  //
  //
  class class_: public type, public scope
  {
  private:
    typedef std::vector<inherits*> inherits_list;

  public:
    typedef inherits_list::const_iterator inherits_iterator;

    inherits_iterator
    inherits_begin () const
    {
      return inherits_.begin ();
    }

    inherits_iterator
    inherits_end () const
    {
      return inherits_.end ();
    }

  public:
    class_ (path const& file, size_t line, size_t column)
        : node (file, line, column)
    {
    }

    void
    add_edge_left (inherits& e)
    {
      inherits_.push_back (&e);
    }

    void
    add_edge_right (inherits&)
    {
    }

    using scope::add_edge_left;
    using scope::add_edge_right;

    // Resolve conflict between scope::scope and nameable::scope.
    //
    using nameable::scope;

  private:
    inherits_list inherits_;
  };
}

#endif // ODB_SEMANTICS_CLASS_HXX
