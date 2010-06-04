// file      : odb/common.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_COMMON_HXX
#define ODB_COMMON_HXX

#include <odb/context.hxx>

// Find id member.
//
struct id_member: traversal::class_,
                  traversal::data_member,
                  context
{
  id_member (context& c)
      : context (c), m_ (0)
  {
    *this >> names_ >> *this;
  }

  semantics::data_member*
  member () const
  {
    return m_;
  }

  virtual void
  traverse (semantics::data_member& m)
  {
    if (m.count ("id"))
      m_ = &m;
  }

  using class_::traverse;

private:
  traversal::names names_;
  semantics::data_member* m_;
};

#endif // ODB_COMMON_HXX
