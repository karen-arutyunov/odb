// file      : odb/common.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_COMMON_HXX
#define ODB_COMMON_HXX

#include <string>
#include <cstddef> // std::size_t

#include <odb/context.hxx>

// Find id member.
//
struct id_member: traversal::class_
{
  id_member ()
  {
    *this >> names_ >> member_;
  }

  semantics::data_member*
  member () const
  {
    return member_.m_;
  }

  virtual void
  traverse (semantics::class_& c)
  {
    member_.m_ = 0;
    names (c);
  }

private:
  struct data_member: traversal::data_member
  {
    virtual void
    traverse (semantics::data_member& m)
    {
      if (m.count ("id"))
        m_ = &m;
    }

    semantics::data_member* m_;
  };

  data_member member_;
  traversal::names names_;
};

// Traverse object columns.
//
struct object_columns_base: traversal::class_
{
  virtual void
  column (semantics::data_member&, std::string const& name, bool first) = 0;

  // If you override this callback, always call the base.
  //
  virtual void
  composite (semantics::data_member&);

  object_columns_base (context& c)
      : member_ (c, *this)
  {
    *this >> names_ >> member_;
    *this >> inherits_ >> *this;
  }

  virtual void
  traverse (semantics::class_&);

private:
  struct member: traversal::data_member, context
  {
    member (context& c, object_columns_base& oc)
        : context (c), oc_ (oc), first_ (true)
    {
    }

    virtual void
    traverse (semantics::data_member&);

  private:
    object_columns_base& oc_;

    string prefix_;
    bool first_;
  };

  member member_;
  traversal::names names_;
  traversal::inherits inherits_;
};

#endif // ODB_COMMON_HXX
