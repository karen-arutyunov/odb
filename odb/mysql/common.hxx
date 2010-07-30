// file      : odb/mysql/common.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_MYSQL_COMMON_HXX
#define ODB_MYSQL_COMMON_HXX

#include <odb/common.hxx>
#include <odb/mysql/context.hxx>

namespace mysql
{
  struct member_base: traversal::data_member, context
  {
    member_base (context& c, bool id)
        : context (c), id_ (id)
    {
    }

    virtual void
    traverse (type& m);

    virtual void
    pre (type&)
    {
    }

    virtual void
    post (type&)
    {
    }

    virtual void
    traverse_integer (type&, sql_type const&)
    {
    }

    virtual void
    traverse_float (type&, sql_type const&)
    {
    }

    virtual void
    traverse_decimal (type&, sql_type const&)
    {
    }

    virtual void
    traverse_date_time (type&, sql_type const&)
    {
    }

    virtual void
    traverse_short_string (type&, sql_type const&)
    {
    }

    virtual void
    traverse_long_string (type&, sql_type const&)
    {
    }

    virtual void
    traverse_bit (type&, sql_type const&)
    {
    }

    virtual void
    traverse_enum (type&, sql_type const&)
    {
    }

    virtual void
    traverse_set (type&, sql_type const&)
    {
    }

  protected:
    bool id_;
    string var;
  };

  struct has_grow_member: member_base
  {
    has_grow_member (context& c)
        : member_base (c, false), r_ (false)
    {
    }

    bool
    result ()
    {
      return r_;
    }

    virtual void
    traverse_long_string (type&, sql_type const& t)
    {
      r_ = true;
    }

    virtual void
    traverse_enum (type&, sql_type const&)
    {
      r_ = true;
    }

    virtual void
    traverse_set (type&, sql_type const&)
    {
      r_ = true;
    }

  private:
    bool r_;
  };
}

#endif // ODB_MYSQL_COMMON_HXX
