// file      : odb/common.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_COMMON_HXX
#define ODB_COMMON_HXX

#include <cstddef> // std::size_t
#include <odb/context.hxx>

// Find id member.
//
struct id_member: traversal::class_,
                  traversal::data_member,
                  context
{
  id_member (context& c)
      : context (c)
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

  virtual void
  traverse (semantics::class_& c)
  {
    m_ = 0;
    names (c);
  }

private:
  traversal::names names_;
  semantics::data_member* m_;
};

// Find id member.
//
struct member_count: traversal::class_,
                     traversal::data_member,
                     context
{
  member_count (context& c)
      : context (c)
  {
    *this >> names_ >> *this;
  }

  std::size_t
  count () const
  {
    return count_;
  }

  virtual void
  traverse (semantics::data_member& m)
  {
    if (!m.count ("transient"))
      count_++;
  }

  virtual void
  traverse (semantics::class_& c)
  {
    count_ = 0;
    names (c);
  }

private:
  traversal::names names_;
  std::size_t count_;
};

#endif // ODB_COMMON_HXX
