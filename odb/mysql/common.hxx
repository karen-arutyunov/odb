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
    member_base (context& c, string const& var = string ())
        : context (c), var_override_ (var), type_override_ (0)
    {
    }

    member_base (context& c,
                 string const& var,
                 semantics::type& type,
                 string const& fq_type,
                 string const& key_prefix)
        : context (c),
          var_override_ (var),
          type_override_ (&type),
          fq_type_override_ (fq_type),
          key_prefix_ (key_prefix)
    {
    }

    virtual void
    traverse (semantics::data_member& m);

    struct member_info
    {
      semantics::data_member& m; // Member.
      semantics::type& t;        // Member C++ type (m.type () may != t).
      sql_type const* st;        // Member SQL type (only simple value types).
      string& var;               // Member variable name with trailing '_'.

      // C++ type fq-name.
      //
      string
      fq_type () const
      {
        // Use the original type from 'm' instead of 't' since the hint
        // may be invalid for a different type. Plus, if a type is
        // overriden, then the fq_type must be as well.
        //
        return fq_type_.empty ()
          ? m.type ().fq_name (m.belongs ().hint ())
          : fq_type_;
      }

      string const& fq_type_;

      member_info (semantics::data_member& m_,
                   semantics::type& t_,
                   string& var_,
                   string const& fq_type)
          : m (m_), t (t_), st (0), var (var_), fq_type_ (fq_type)
      {
      }
    };

    virtual void
    pre (member_info&)
    {
    }

    virtual void
    post (member_info&)
    {
    }

    virtual void
    traverse_composite (member_info&)
    {
    }

    virtual void
    traverse_container (member_info&)
    {
    }

    virtual void
    traverse_object_pointer (member_info& mi)
    {
      traverse_simple (mi);
    }

    virtual void
    traverse_simple (member_info&);

    virtual void
    traverse_integer (member_info&)
    {
    }

    virtual void
    traverse_float (member_info&)
    {
    }

    virtual void
    traverse_decimal (member_info&)
    {
    }

    virtual void
    traverse_date_time (member_info&)
    {
    }

    virtual void
    traverse_string (member_info&)
    {
    }

    virtual void
    traverse_short_string (member_info& mi)
    {
      traverse_string (mi);
    }

    virtual void
    traverse_long_string (member_info& mi)
    {
      traverse_string (mi);
    }

    virtual void
    traverse_bit (member_info&)
    {
    }

    virtual void
    traverse_enum (member_info&)
    {
    }

    virtual void
    traverse_set (member_info&)
    {
    }

  protected:
    string var_override_;
    semantics::type* type_override_;
    string fq_type_override_;
    string key_prefix_;
  };

  struct member_image_type: member_base
  {
    member_image_type (context&);

    member_image_type (context& c,
                       semantics::type& type,
                       string const& fq_type,
                       string const& key_prefix);

    string
    image_type (semantics::data_member&);

    virtual void
    traverse_composite (member_info&);

    virtual void
    traverse_integer (member_info&);

    virtual void
    traverse_float (member_info&);

    virtual void
    traverse_decimal (member_info&);

    virtual void
    traverse_date_time (member_info&);

    virtual void
    traverse_string (member_info&);

    virtual void
    traverse_bit (member_info&);

    virtual void
    traverse_enum (member_info&);

    virtual void
    traverse_set (member_info&);

  private:
    string type_;
  };

  struct member_database_type: member_base
  {
    member_database_type (context&);

    member_database_type (context& c,
                          semantics::type& type,
                          string const& fq_type,
                          string const& key_prefix);

    string
    database_type (type&);

    virtual void
    traverse_composite (member_info&);

    virtual void
    traverse_integer (member_info&);

    virtual void
    traverse_float (member_info&);

    virtual void
    traverse_decimal (member_info&);

    virtual void
    traverse_date_time (member_info&);

    virtual void
    traverse_string (member_info&);

    virtual void
    traverse_bit (member_info&);

    virtual void
    traverse_enum (member_info&);

    virtual void
    traverse_set (member_info&);

  private:
    string type_;
  };

  struct query_columns: object_columns_base, context
  {
    query_columns (context&);
    query_columns (context&, semantics::class_&);

    virtual void
    composite (semantics::data_member&, semantics::type&);

    virtual void
    column (semantics::data_member&, string const&, bool);

  private:
    string scope_;
    string table_;
    bool decl_;

    member_image_type member_image_type_;
    member_database_type member_database_type_;
  };
}

#endif // ODB_MYSQL_COMMON_HXX
