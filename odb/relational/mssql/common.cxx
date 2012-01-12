// file      : odb/relational/mssql/common.cxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>

#include <odb/relational/mssql/common.hxx>

using namespace std;

namespace relational
{
  namespace mssql
  {
    //
    // member_base
    //

    void member_base::
    traverse (semantics::data_member& m)
    {
      if (transient (m))
        return;

      string var;

      if (!var_override_.empty ())
        var = var_override_;
      else
      {
        string const& name (m.name ());
        var = name + (name[name.size () - 1] == '_' ? "" : "_");
      }

      bool cq (type_override_ != 0 ? false : const_type (m.type ()));
      semantics::type& t (type_override_ != 0 ? *type_override_ : utype (m));

      semantics::type* cont;
      if (semantics::class_* c = composite_wrapper (t))
      {
        // If t is a wrapper, pass the wrapped type. Also pass the
        // original, wrapper type.
        //
        member_info mi (m,
                        *c,
                        (wrapper (t) ? &t : 0),
                        cq,
                        var,
                        fq_type_override_);
        if (pre (mi))
        {
          traverse_composite (mi);
          post (mi);
        }
      }
      // This cannot be a container if we have a type override.
      //
      else if (type_override_ == 0 && (cont = context::container (m)))
      {
        // The same unwrapping logic as for composite values.
        //
        member_info mi (m,
                        *cont,
                        (wrapper (t) ? &t : 0),
                        cq,
                        var,
                        fq_type_override_);
        if (pre (mi))
        {
          traverse_container (mi);
          post (mi);
        }
      }
      else
      {
        sql_type const& st (column_sql_type (m, key_prefix_));

        if (semantics::class_* c = object_pointer (t))
        {
          member_info mi (m,
                          utype (*id_member (*c)),
                          0,
                          cq,
                          var,
                          fq_type_override_);
          mi.st = &st;
          if (pre (mi))
          {
            traverse_object_pointer (mi);
            post (mi);
          }
        }
        else
        {
          member_info mi (m, t, 0, cq, var, fq_type_override_);
          mi.st = &st;
          if (pre (mi))
          {
            traverse_simple (mi);
            post (mi);
          }
        }
      }
    }

    void member_base::
    traverse_simple (member_info& mi)
    {
      const sql_type& st (*mi.st);

      switch (st.type)
      {
        // Integral types.
        //
      case sql_type::BIT:
      case sql_type::TINYINT:
      case sql_type::SMALLINT:
      case sql_type::INT:
      case sql_type::BIGINT:
        {
          traverse_integer (mi);
          break;
        }

        // Fixed and floating point types.
        //
      case sql_type::DECIMAL:
        {
          traverse_decimal (mi);
          break;
        }
      case sql_type::SMALLMONEY:
        {
          traverse_smallmoney (mi);
          break;
        }
      case sql_type::MONEY:
        {
          traverse_money (mi);
          break;
        }
      case sql_type::FLOAT:
        {
          if (st.prec > 24)
            traverse_float8 (mi);
          else
            traverse_float4 (mi);

          break;
        }

        // String and binary types.
        //
      case sql_type::CHAR:
      case sql_type::VARCHAR:
        {
          // Zero precision means max in VARCHAR(max).
          //
          if (st.prec == 0 || st.prec > options.mssql_short_limit ())
            traverse_long_string (mi);
          else
            traverse_string (mi);

          break;
        }
      case sql_type::TEXT:
        {
          traverse_long_string (mi);
          break;
        }
      case sql_type::NCHAR:
      case sql_type::NVARCHAR:
        {
          // Zero precision means max in NVARCHAR(max). Note that
          // the precision is in 2-byte UCS-2 characters, not bytes.
          //
          if (st.prec == 0 || st.prec * 2 > options.mssql_short_limit ())
            traverse_long_nstring (mi);
          else
            traverse_nstring (mi);

          break;
        }
      case sql_type::NTEXT:
        {
          traverse_long_nstring (mi);
          break;
        }
      case sql_type::BINARY:
      case sql_type::VARBINARY:
        {
          // Zero precision means max in VARCHAR(max).
          //
          if (st.prec == 0 || st.prec > options.mssql_short_limit ())
            traverse_long_binary (mi);
          else
            traverse_binary (mi);

          break;
        }
      case sql_type::IMAGE:
        {
          traverse_long_binary (mi);
          break;
        }

        // Date-time types.
        //
      case sql_type::DATE:
        {
          traverse_date (mi);
          break;
        }
      case sql_type::TIME:
        {
          traverse_time (mi);
          break;
        }
      case sql_type::DATETIME:
      case sql_type::DATETIME2:
      case sql_type::SMALLDATETIME:
        {
          traverse_datetime (mi);
          break;
        }
      case sql_type::DATETIMEOFFSET:
        {
          traverse_datetimeoffset (mi);
          break;
        }

        // Other types.
        //
      case sql_type::UNIQUEIDENTIFIER:
        {
          traverse_uniqueidentifier (mi);
          break;
        }
      case sql_type::ROWVERSION:
        {
          traverse_rowversion (mi);
          break;
        }
      case sql_type::invalid:
        {
          assert (false);
          break;
        }
      }
    }

    //
    // member_image_type
    //

    static const char* integer_types[] =
    {
      "unsigned char",
      "unsigned char",
      "short",
      "int",
      "long long"
    };

    member_image_type::
    member_image_type (semantics::type* type,
                       string const& fq_type,
                       string const& key_prefix)
        : relational::member_base (type, fq_type, key_prefix)
    {
    }

    string member_image_type::
    image_type (semantics::data_member& m)
    {
      type_.clear ();
      member_base::traverse (m);
      return type_;
    }

    void member_image_type::
    traverse_composite (member_info& mi)
    {
      type_ = "composite_value_traits< " + mi.fq_type () + " >::image_type";
    }

    void member_image_type::
    traverse_integer (member_info& mi)
    {
      type_ = integer_types[mi.st->type - sql_type::BIT];
    }

    void member_image_type::
    traverse_decimal (member_info&)
    {
      type_ = "mssql::decimal";
    }

    void member_image_type::
    traverse_smallmoney (member_info&)
    {
      type_ = "mssql::smallmoney";
    }

    void member_image_type::
    traverse_money (member_info&)
    {
      type_ = "mssql::money";
    }

    void member_image_type::
    traverse_float4 (member_info&)
    {
      type_ = "float";
    }

    void member_image_type::
    traverse_float8 (member_info&)
    {
      type_ = "double";
    }

    void member_image_type::
    traverse_string (member_info&)
    {
      type_ = "char*";
    }

    void member_image_type::
    traverse_long_string (member_info&)
    {
      type_ = "mssql::long_callback";
    }

    void member_image_type::
    traverse_nstring (member_info&)
    {
      type_ = "mssql::ucs2_char*";
    }

    void member_image_type::
    traverse_long_nstring (member_info&)
    {
      type_ = "mssql::long_callback";
    }

    void member_image_type::
    traverse_binary (member_info&)
    {
      type_ = "char*";
    }

    void member_image_type::
    traverse_long_binary (member_info&)
    {
      type_ = "mssql::long_callback";
    }

    void member_image_type::
    traverse_date (member_info&)
    {
      type_ = "mssql::date";
    }

    void member_image_type::
    traverse_time (member_info&)
    {
      type_ = "mssql::time";
    }

    void member_image_type::
    traverse_datetime (member_info&)
    {
      type_ = "mssql::datetime";
    }

    void member_image_type::
    traverse_datetimeoffset (member_info&)
    {
      type_ = "mssql::datetimeoffset";
    }

    void member_image_type::
    traverse_uniqueidentifier (member_info&)
    {
      type_ = "mssql::uniqueidentifier";
    }

    void member_image_type::
    traverse_rowversion (member_info&)
    {
      type_ = "unsigned char*";
    }

    //
    // member_database_type
    //

    static const char* integer_database_id[] =
    {
      "mssql::id_bit",
      "mssql::id_tinyint",
      "mssql::id_smallint",
      "mssql::id_int",
      "mssql::id_bigint"
    };

    member_database_type_id::
    member_database_type_id (semantics::type* type,
                             string const& fq_type,
                             string const& key_prefix)
        : relational::member_base (type, fq_type, key_prefix)
    {
    }

    string member_database_type_id::
    database_type_id (type& m)
    {
      type_id_.clear ();
      member_base::traverse (m);
      return type_id_;
    }

    void member_database_type_id::
    traverse_composite (member_info&)
    {
      assert (false);
    }

    void member_database_type_id::
    traverse_integer (member_info& mi)
    {
      type_id_ = integer_database_id[mi.st->type - sql_type::BIT];
    }

    void member_database_type_id::
    traverse_decimal (member_info&)
    {
      type_id_ = "mssql::id_decimal";
    }

    void member_database_type_id::
    traverse_smallmoney (member_info&)
    {
      type_id_ = "mssql::id_smallmoney";
    }

    void member_database_type_id::
    traverse_money (member_info&)
    {
      type_id_ = "mssql::id_money";
    }

    void member_database_type_id::
    traverse_float4 (member_info&)
    {
      type_id_ = "mssql::id_float4";
    }

    void member_database_type_id::
    traverse_float8 (member_info&)
    {
      type_id_ = "mssql::id_float8";
    }

    void member_database_type_id::
    traverse_string (member_info&)
    {
      type_id_ = "mssql::id_string";
    }

    void member_database_type_id::
    traverse_long_string (member_info&)
    {
      type_id_ = "mssql::id_long_string";
    }

    void member_database_type_id::
    traverse_nstring (member_info&)
    {
      type_id_ = "mssql::id_nstring";
    }

    void member_database_type_id::
    traverse_long_nstring (member_info&)
    {
      type_id_ = "mssql::id_long_nstring";
    }

    void member_database_type_id::
    traverse_binary (member_info&)
    {
      type_id_ = "mssql::id_binary";
    }

    void member_database_type_id::
    traverse_long_binary (member_info&)
    {
      type_id_ = "mssql::id_long_binary";
    }

    void member_database_type_id::
    traverse_date (member_info&)
    {
      type_id_ = "mssql::id_date";
    }

    void member_database_type_id::
    traverse_time (member_info&)
    {
      type_id_ = "mssql::id_time";
    }

    void member_database_type_id::
    traverse_datetime (member_info&)
    {
      type_id_ = "mssql::id_datetime";
    }

    void member_database_type_id::
    traverse_datetimeoffset (member_info&)
    {
      type_id_ = "mssql::id_datetimeoffset";
    }

    void member_database_type_id::
    traverse_uniqueidentifier (member_info&)
    {
      type_id_ = "mssql::id_uniqueidentifier";
    }

    void member_database_type_id::
    traverse_rowversion (member_info&)
    {
      type_id_ = "mssql::id_rowversion";
    }

    //
    // query_columns
    //

    struct query_columns: relational::query_columns, context
    {
      query_columns (base const& x): base (x) {}

      virtual string
      database_type_id (semantics::data_member& m)
      {
        return member_database_type_id_.database_type_id (m);
      }

      virtual void
      column_ctor (string const& type, string const& base)
      {
        os << type << " (const char* t," << endl
           << "const char* c," << endl
           << "unsigned short p = 0," << endl
           << "unsigned short s = 0xFFFF)" << endl
           << "  : " << base << " (t, c, p, s)"
           << "{"
           << "}";
      }

      virtual void
      column_ctor_extra (semantics::data_member& m)
      {
        // For some types we need to pass precision and scale.
        //
        sql_type const& st (column_sql_type (m));

        switch (st.type)
        {
        case sql_type::DECIMAL:
          {
            os << ", " << st.prec << ", " << st.scale;
            break;
          }
        case sql_type::FLOAT:
          {
            os << ", " << st.prec;
            break;
          }
        case sql_type::CHAR:
        case sql_type::VARCHAR:
          {
            os << ", " << st.prec;
            break;
          }
        case sql_type::TEXT:
          {
            os << ", 0"; // Unlimited.
            break;
          }
        case sql_type::NCHAR:
        case sql_type::NVARCHAR:
          {
            os << ", " << st.prec; // In 2-byte characters.
            break;
          }
        case sql_type::NTEXT:
          {
            os << ", 0"; // Unlimited.
            break;
          }
        case sql_type::BINARY:
        case sql_type::VARBINARY:
          {
            os << ", " << st.prec;
            break;
          }
        case sql_type::IMAGE:
          {
            os << ", 0"; // Unlimited.
            break;
          }
          // Date-time types.
          //
        case sql_type::TIME:
        case sql_type::DATETIME2:
        case sql_type::DATETIMEOFFSET:
          {
            os << ", 0, " << st.scale; // Fractional seconds (scale).
            break;
          }
        case sql_type::DATETIME:
          {
            os << ", 0, 3";
            break;
          }
        case sql_type::SMALLDATETIME:
          {
            os << ", 0, 8";
            break;
          }
        default:
          {
            break;
          }
        }
      }

    private:
      member_database_type_id member_database_type_id_;
    };
    entry<query_columns> query_columns_;
  }
}
