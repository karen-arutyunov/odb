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

// Count persistent members.
//
struct member_count: traversal::class_
{
  member_count ()
  {
    *this >> names_ >> member_;
  }

  std::size_t
  count () const
  {
    return member_.count_;
  }

  virtual void
  traverse (semantics::class_& c)
  {
    member_.count_ = 0;
    names (c);
  }

private:
  struct data_member: traversal::data_member
  {
    virtual void
    traverse (semantics::data_member& m)
    {
      if (!m.count ("transient"))
        count_++;
    }

    std::size_t count_;
  };

  data_member member_;
  traversal::names names_;
};

#endif // ODB_COMMON_HXX
