// file      : odb/mysql/common.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

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
    traverse_string (type&, sql_type const&)
    {
    }

    virtual void
    traverse_short_string (type& t, sql_type const& st)
    {
      traverse_string (t, st);
    }

    virtual void
    traverse_long_string (type& t, sql_type const& st)
    {
      traverse_string (t, st);
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

  struct member_image_type: member_base
  {
    member_image_type (context&, bool id);

    string
    image_type (type&);

    virtual void
    traverse_integer (type&, sql_type const&);

    virtual void
    traverse_float (type&, sql_type const&);

    virtual void
    traverse_decimal (type&, sql_type const&);

    virtual void
    traverse_date_time (type&, sql_type const&);

    virtual void
    traverse_string (type&, sql_type const&);

    virtual void
    traverse_bit (type&, sql_type const&);

    virtual void
    traverse_enum (type&, sql_type const&);

    virtual void
    traverse_set (type&, sql_type const&);

  private:
    string type_;
  };

  struct member_database_type: member_base
  {
    member_database_type (context&);

    string
    database_type (type&);

    virtual void
    traverse_integer (type&, sql_type const&);

    virtual void
    traverse_float (type&, sql_type const&);

    virtual void
    traverse_decimal (type&, sql_type const&);

    virtual void
    traverse_date_time (type&, sql_type const&);

    virtual void
    traverse_string (type&, sql_type const&);

    virtual void
    traverse_bit (type&, sql_type const&);

    virtual void
    traverse_enum (type&, sql_type const&);

    virtual void
    traverse_set (type&, sql_type const&);

  private:
    string type_;
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
    traverse_decimal (type&, sql_type const& t)
    {
      r_ = true;
    }

    virtual void
    traverse_long_string (type&, sql_type const& t)
    {
      r_ = true;
    }

    virtual void
    traverse_short_string (type&, sql_type const& t)
    {
      r_ = true; // @@ Short string optimization disabled.
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

  struct query_column: traversal::data_member, context
  {
    query_column (context&);
    query_column (context&, semantics::class_&);

    virtual void
    traverse (type&);

  private:
    string scope_;
    string table_;
    bool decl_;

    member_image_type member_image_type_;
    member_database_type member_database_type_;
  };
}

#endif // ODB_MYSQL_COMMON_HXX
